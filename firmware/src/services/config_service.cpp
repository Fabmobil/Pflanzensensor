/**
 * @file config_service.cpp
 */
#include "services/config_service.h"

#include <new>

#include "core/util/base64.h"
#include "core/util/sha256.h"
#include "hal/system.h"
#include "services/log.h"
#include "services/storage.h"

namespace ps {

namespace {
template <typename T> struct WriteCtx {
  const T* obj;
};

bool writeDeviceFn(hal::File& f, void* c) {
  JsonWriter w(f);
  doc::writeDevice(w, *static_cast<const DeviceConfig*>(c));
  return true;
}

bool writeSensorsFn(hal::File& f, void* c) {
  JsonWriter w(f);
  w.beginObject().kv(FS_("schema"), doc::kSchemaVersion).key(FS_("sensors"));
  doc::writeSensors(w, *static_cast<const SensorsConfig*>(c));
  w.endObject();
  return true;
}

bool writeSecretsFn(hal::File& f, void* c) {
  JsonWriter w(f);
  doc::writeSecrets(w, *static_cast<const Secrets*>(c));
  return true;
}

void logWarnings(const char* file, const schema::FieldErrors& w) {
  for (uint8_t i = 0; i < w.count; i++) {
    char code[16];
    copyP(code, sizeof(code), errCode(w.items[i].err));
    LOGW(Mod::Config, "%s: %s ungültig (%s) - Standard bleibt", file, w.items[i].path.c_str(),
         code);
  }
}
} // namespace

void ConfigService::load() {
  defaults::device(dev_);
  defaults::secrets(sec_);
  sensorDefaults(sens_);

  {
    JsonDocument doc;
    if (storage::readJson(fs_, kDevicePath, doc)) {
      schema::FieldErrors w;
      doc::readDevice(doc.as<JsonObjectConst>(), dev_, w);
      logWarnings(kDevicePath, w);
    }
  }
  {
    JsonDocument doc;
    if (storage::readJson(fs_, kSensorsPath, doc)) {
      schema::FieldErrors w;
      doc::readSensors(doc.as<JsonObjectConst>(), sens_, w);
      logWarnings(kSensorsPath, w);
    }
  }
  {
    JsonDocument doc;
    if (storage::readJson(fs_, kSecretsPath, doc))
      doc::readSecrets(doc.as<JsonObjectConst>(), sec_);
  }
  LOGI(Mod::Config, "Konfiguration geladen: %s", dev_.deviceName.c_str());
}

Err ConfigService::patchDevice(JsonObjectConst patch, schema::FieldErrors& errs, uint8_t& changed) {
  // Kopie nicht auf dem Stack (≈ 700 B, Grenze 512 B je Rahmen)
  hal::TempBuffer mem(sizeof(DeviceConfig));
  if (!mem)
    return Err::NoSpace;
  DeviceConfig& copy = *new (mem.data()) DeviceConfig(dev_);
  schema::apply(schema::deviceTable(), patch, &copy, errs, true);
  if (!errs.empty())
    return Err::InvalidArgument;
  if (copy.ledMode == 1 && !board::kFeatures.display) {
    errs.add("led.mode", Err::Unsupported);
    return Err::InvalidArgument;
  }
  changed = 0;
  // Geänderte Bereiche ermitteln (über die Schematabelle)
  const schema::Table& t = schema::deviceTable();
  for (uint8_t i = 0; i < t.count; i++) {
    const schema::FieldDef f = schema::field(t, i);
    const uint8_t* a = reinterpret_cast<const uint8_t*>(&dev_) + f.offset;
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&copy) + f.offset;
    if (memcmp(a, b, f.size) == 0)
      continue;
    if (strcmp(f.section, "general") == 0)
      changed |= SecGeneral;
    if (strcmp(f.section, "wifi") == 0)
      changed |= SecWifi;
    if (strcmp(f.section, "display") == 0)
      changed |= SecDisplay;
    if (strcmp(f.section, "led") == 0)
      changed |= SecLed;
    if (strcmp(f.section, "log") == 0)
      changed |= SecLog;
    if (strcmp(f.section, "mail") == 0)
      changed |= SecMail;
  }
  dev_ = copy;
  copy.~DeviceConfig();
  if (changed)
    markDeviceDirty(hal::now());
  return Err::None;
}

Err ConfigService::patchSensors(JsonObjectConst patch, schema::FieldErrors& errs,
                                uint32_t& changedSensors) {
  hal::TempBuffer mem(sizeof(SensorsConfig));
  if (!mem)
    return Err::NoSpace;
  SensorsConfig& copy = *new (mem.data()) SensorsConfig(sens_);
  changedSensors = 0;
  doc::applySensors(patch, copy, errs, true, &changedSensors);
  if (!errs.empty())
    return Err::InvalidArgument;
  sens_ = copy;
  if (changedSensors)
    markSensorsDirty(hal::now());
  return Err::None;
}

void ConfigService::markSensorsDirty(Instant now) {
  if (!sensDirty_)
    sensSince_ = now;
  sensDirty_ = true;
  sensSince_ = now; // Debounce: ab der letzten Änderung
}

void ConfigService::markDeviceDirty(Instant now) {
  devDirty_ = true;
  devSince_ = now;
}

Err ConfigService::setWifi(uint8_t slot, const char* ssid, const char* password,
                           bool keepPassword) {
  if (slot >= 3 || !ssid)
    return Err::InvalidArgument;
  if (strlen(ssid) > 32)
    return Err::TooLong;
  if (password && strlen(password) > 64)
    return Err::TooLong;
  dev_.wifiSsid[slot].assign(ssid);
  if (!keepPassword)
    sec_.wifiPassword[slot].assign(password ? password : "");
  if (!*ssid)
    sec_.wifiPassword[slot].clear();
  markDeviceDirty(hal::now());
  secDirty_ = true;
  return Err::None;
}

bool ConfigService::adminPasswordValid(const char* pw) {
  if (!pw || !*pw)
    return false;
  size_t n = 0;
  for (const char* p = pw; *p; p++, n++)
    if (static_cast<uint8_t>(*p) < 0x20 || static_cast<uint8_t>(*p) > 0x7E)
      return false;
  return n <= 32;
}

void ConfigService::hashPassword(const char* pw, const uint8_t salt[16], uint8_t out[32]) const {
  Sha256 h;
  h.update(salt, 16);
  h.update(pw);
  h.finish(out);
}

Err ConfigService::setAdminPassword(const char* pw) {
  if (!adminPasswordValid(pw))
    return Err::InvalidArgument;
  hal::fillRandom(sec_.adminSalt, sizeof(sec_.adminSalt));
  hashPassword(pw, sec_.adminSalt, sec_.adminHash);
  sec_.adminSet = true;
  secDirty_ = true;
  return Err::None;
}

bool ConfigService::checkAdminPassword(const char* pw) const {
  if (!pw)
    return false;
  if (!sec_.adminSet)
    return strcmp(pw, defaults::kAdminPassword) == 0;
  uint8_t h[32];
  hashPassword(pw, sec_.adminSalt, h);
  return equalConstTime(h, sec_.adminHash, 32);
}

Err ConfigService::setMailKey(const char* key) {
  if (!key)
    return Err::InvalidArgument;
  uint8_t raw[40];
  if (base64::decode(key, strlen(key), raw, sizeof(raw)) != 32)
    return Err::InvalidArgument;
  sec_.mailKey.assign(key);
  secDirty_ = true;
  return Err::None;
}

bool ConfigService::saveDevice() {
  return storage::writeAtomic(fs_, kDevicePath, writeDeviceFn, &dev_);
}
bool ConfigService::saveSensors() {
  return storage::writeAtomic(fs_, kSensorsPath, writeSensorsFn, &sens_);
}
bool ConfigService::saveSecrets() {
  return storage::writeAtomic(fs_, kSecretsPath, writeSecretsFn, &sec_);
}

void ConfigService::tick(Instant now, bool force) {
  if (devDirty_ && (force || (now - devSince_).millis() >= static_cast<int32_t>(kDebounceMs))) {
    if (saveDevice())
      LOGD(Mod::Config, "Gerätekonfiguration gespeichert");
    devDirty_ = false;
  }
  if (sensDirty_ && (force || (now - sensSince_).millis() >= static_cast<int32_t>(kDebounceMs))) {
    if (saveSensors())
      LOGD(Mod::Config, "Sensorkonfiguration gespeichert");
    sensDirty_ = false;
  }
  if (secDirty_) {
    if (saveSecrets())
      LOGD(Mod::Config, "Geheimnisse gespeichert");
    secDirty_ = false;
  }
}

void ConfigService::exportTo(JsonWriter& w) {
  w.beginObject();
  w.kv(FS_("schema"), doc::kSchemaVersion);
  w.key(FS_("device"));
  doc::writeDevice(w, dev_);
  w.key(FS_("sensors"));
  doc::writeSensors(w, sens_);
  w.key(FS_("templates"));
  templates_.writeCustomJson(w);
  w.endObject();
}

ConfigService::ImportResult ConfigService::importFile() {
  ImportResult res;
  JsonDocument doc;
  {
    hal::FileGuard f(fs_, fs_.open(kImportPath, hal::OpenMode::Read));
    if (!f) {
      res.err = Err::NotFound;
      return res;
    }
    storage::FileReader r(*f.get());
    if (deserializeJson(doc, r) != DeserializationError::Ok) {
      res.err = Err::ParseError;
      fs_.remove(kImportPath);
      return res;
    }
  }
  fs_.remove(kImportPath);
  JsonObjectConst root = doc.as<JsonObjectConst>();

  // Arbeitskopien nicht auf dem Stack
  struct Work {
    DeviceConfig dev;
    SensorsConfig sens;
    Secrets sec;
    schema::FieldErrors errs;
    schema::FieldErrors tplErrs;
  };
  hal::TempBuffer mem(sizeof(Work));
  if (!mem) {
    res.err = Err::NoSpace;
    return res;
  }
  Work& work = *new (mem.data()) Work{dev_, sens_, sec_, {}, {}};
  DeviceConfig& dev = work.dev;
  SensorsConfig& sens = work.sens;
  Secrets& sec = work.sec;

  if (legacy::isLegacy(root)) {
    res.legacyFormat = true;
    legacy::Extras* ex = static_cast<legacy::Extras*>(hal::tempAlloc(sizeof(legacy::Extras)));
    if (!ex) {
      res.err = Err::NoSpace;
      return res;
    }
    new (ex) legacy::Extras();
    legacy::import(root, dev, sens, sec, *ex, res.report);
    if (ex->hasAdminPassword) {
      sec_ = sec;
      setAdminPassword(ex->adminPassword.c_str());
      sec = sec_;
    }
    for (uint8_t i = 0; i < board::kSlotCount; i++)
      if (ex->hasState[i])
        state_.channel(i) = ex->state[i];
    ex->~Extras();
    hal::tempFree(ex);
  } else if (root["schema"].as<int>() == doc::kSchemaVersion) {
    schema::FieldErrors& w = work.errs;
    JsonObjectConst d = root["device"].as<JsonObjectConst>();
    if (!d.isNull())
      res.report.applied += schema::apply(schema::deviceTable(), d, &dev, w, true);
    JsonObjectConst s = root["sensors"].as<JsonObjectConst>();
    if (!s.isNull())
      res.report.applied += doc::applySensors(s, sens, w, true);
    for (uint8_t i = 0; i < w.count; i++) {
      if (w.items[i].err == Err::NotFound)
        res.report.ignore(w.items[i].path.c_str());
      else
        res.report.bad(w.items[i].path.c_str());
    }
    res.report.invalid = static_cast<uint16_t>(res.report.invalid + w.dropped);
    JsonObjectConst tpl = root["templates"].as<JsonObjectConst>();
    for (JsonPairConst kv : tpl) {
      TemplateKind k;
      if (!TemplateStore::kindFromName(kv.key().c_str(), k)) {
        res.report.ignore(kv.key().c_str());
        continue;
      }
      schema::FieldErrors& te = work.tplErrs;
      te = schema::FieldErrors{};
      if (templates_.save(k, kv.value()["subject"] | "", kv.value()["body"] | "", te) == Err::None)
        res.report.applied++;
      else
        res.report.bad(kv.key().c_str());
    }
  } else {
    res.err = Err::ParseError;
    return res;
  }

  dev_ = dev;
  sens_ = sens;
  sec_ = sec;
  saveDevice();
  saveSensors();
  saveSecrets();
  state_.save();
  devDirty_ = sensDirty_ = secDirty_ = false;
  LOGI(Mod::Config, "Import: %u übernommen, %u ignoriert, %u ungültig",
       static_cast<unsigned>(res.report.applied), static_cast<unsigned>(res.report.ignored),
       static_cast<unsigned>(res.report.invalid));
  return res;
}

void ConfigService::factoryReset() {
  const char* files[] = {kDevicePath,        kSensorsPath,         kSecretsPath,
                         StateStore::kPath,  TemplateStore::kPath, "/device.cfg.tmp",
                         "/sensors.cfg.tmp", "/secrets.cfg.tmp",   "/state.bin.tmp",
                         "/mail.tpl.tmp",    kImportPath};
  for (const char* f : files)
    fs_.remove(f);
  devDirty_ = sensDirty_ = secDirty_ = false;
  LOGW(Mod::Config, "Werksreset: Konfiguration gelöscht");
}

} // namespace ps
