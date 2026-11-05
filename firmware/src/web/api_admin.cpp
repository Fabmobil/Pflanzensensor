/**
 * @file api_admin.cpp
 * @brief Admin-API: Konfiguration, Geheimnisse, System, Logs, Sensoren, Mail, OTA
 */
#include <stdio.h>
#include <stdlib.h>

#include "core/version.h"
#include "hal/system.h"
#include "web/handlers.h"
#include "web/respond.h"

namespace ps {
namespace web {

namespace {

void pushEvent(WebContext& c, EventType t, uint8_t a = 0, uint32_t v = 0) {
  Event e;
  e.type = t;
  e.a = a;
  e.v = v;
  c.events.push(e);
}

const char* healthName(Health h) {
  switch (h) {
  case Health::Ok:
    return "ok";
  case Health::Degraded:
    return "degraded";
  default:
    return "failed";
  }
}

void writeChannelList(JsonWriter& w, const SensorsConfig& sc) {
  w.key(FS_("channels")).beginArray();
  for (uint8_t s = 0; s < board::kSlotCount; s++) {
    char key[32];
    schema::keyOfSlot(s, key, sizeof(key));
    w.beginObject();
    w.kv(FS_("key"), static_cast<const char*>(key));
    w.kv(FS_("name"), sc.channels[s].name.c_str());
    w.kv(FS_("enabled"), sc.channels[s].enabled && sc.sensors[board::kSlots[s].sensor].enabled);
    w.endObject();
  }
  w.endArray();
}

} // namespace

// ---------------------------------------------------------------------------
// /api/v1/config  GET | PATCH
// ---------------------------------------------------------------------------
void configGet(WebContext& c, hal::Request& req, hal::Response& res, const char* rest) {
  if (req.method() == hal::HttpMethod::Patch) {
    configPatch(c, req, res, rest);
    return;
  }
  beginJson(res);
  JsonWriter w(res);
  const Secrets& sec = c.cfg.secrets();
  w.beginObject();
  w.kv(FS_("ok"), true);
  w.key(FS_("config"));
  doc::writeDevice(w, c.cfg.device());
  w.key(FS_("secrets")).beginObject();
  w.key(FS_("admin")).beginObject().kv(FS_("set"), sec.adminSet).endObject();
  w.key(FS_("wifi")).beginArray();
  for (const auto& p : sec.wifiPassword)
    w.beginObject().kv(FS_("set"), !p.empty()).endObject();
  w.endArray();
  w.key(FS_("mailKey")).beginObject().kv(FS_("set"), !sec.mailKey.empty()).endObject();
  w.endObject();
  w.kv(FS_("activeWifiSlot"), static_cast<int32_t>(c.net.activeSlot()));
  w.key(FS_("features")).beginObject();
  w.kv(FS_("display"), board::kFeatures.display);
  w.kv(FS_("led"), board::kFeatures.ledTraffic);
  w.endObject();
  writeChannelList(w, c.cfg.sensors());
  w.key(FS_("system")).beginObject();
  c.sys.writeHeap(w);
  w.kv(FS_("uptime"), c.sys.uptimeS());
  char buf[33];
  c.net.ssid(buf, sizeof(buf));
  w.kv(FS_("ssid"), static_cast<const char*>(buf));
  w.kv(FS_("rssi"), static_cast<int32_t>(c.net.rssi()));
  c.net.ip(buf, sizeof(buf));
  w.kv(FS_("ip"), static_cast<const char*>(buf));
  hal::macString(buf);
  w.kv(FS_("mac"), static_cast<const char*>(buf));
  const hal::FsInfo& fi = c.sys.fsInfo();
  w.kv(FS_("fsTotal"), fi.total);
  w.kv(FS_("fsUsed"), fi.used);
  w.kv(FS_("logFileSize"), c.log.fileSize());
  w.kv(FS_("logFileMax"), LogService::kMaxFileSize);
  w.kv(FS_("version"), kVersion);
  w.kv(FS_("buildDate"), kBuildDate);
  w.endObject();
  w.endObject();
  res.end();
}

void configPatch(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  JsonObjectConst patch = doc.as<JsonObjectConst>();
  // Der aktive WLAN-Slot ist nicht editierbar
  const int8_t active = c.net.activeSlot();
  if (active >= 0) {
    char k[8];
    snprintf(k, sizeof(k), "ssid%d", active + 1);
    if (!patch["wifi"][k].isNull()) {
      schema::FieldErrors fe;
      char path[16];
      snprintf(path, sizeof(path), "wifi.%s", k);
      fe.add(path, Err::Busy);
      sendError(res, 409, Err::Busy, "Der aktive WLAN-Slot ist nicht änderbar", &fe);
      return;
    }
  }
  schema::FieldErrors errs;
  uint8_t changed = 0;
  const Err e = c.cfg.patchDevice(patch, errs, changed);
  if (e != Err::None) {
    sendError(res, 400, e, "Ungültige Werte", &errs);
    return;
  }
  if (changed)
    pushEvent(c, EventType::ConfigChanged, changed);
  beginJson(res);
  JsonWriter w(res);
  w.beginObject()
      .kv(FS_("ok"), true)
      .kv(FS_("changed"), static_cast<uint32_t>(changed))
      .endObject();
  res.end();
}

void configExport(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  res.header("Content-Disposition", "attachment; filename=pflanzensensor-config.json");
  beginJson(res);
  JsonWriter w(res);
  c.cfg.exportTo(w);
  res.end();
}

void configImport(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  const ConfigService::ImportResult r = c.cfg.importFile();
  if (r.err != Err::None) {
    sendError(res, statusFor(r.err), r.err,
              r.err == Err::NotFound ? "Keine Datei empfangen" : "Datei nicht lesbar");
    return;
  }
  beginJson(res);
  JsonWriter w(res);
  w.beginObject();
  w.kv(FS_("ok"), true);
  w.kv(FS_("legacy"), r.legacyFormat);
  w.kv(FS_("applied"), static_cast<uint32_t>(r.report.applied));
  w.kv(FS_("ignored"), static_cast<uint32_t>(r.report.ignored));
  w.kv(FS_("invalid"), static_cast<uint32_t>(r.report.invalid));
  w.key(FS_("notes"));
  r.report.notes.writeJson(w);
  w.kv(FS_("needsReboot"), true);
  w.endObject();
  res.end();
  pushEvent(c, EventType::RebootRequested, 1);
}

// ---------------------------------------------------------------------------
// Geheimnisse (werden nie zurückgegeben)
// ---------------------------------------------------------------------------
void secretAdmin(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  const char* pw = doc["password"] | "";
  if (c.cfg.setAdminPassword(pw) != Err::None) {
    sendError(res, 400, Err::InvalidArgument,
              "Passwort: 1 bis 32 Zeichen, nur druckbare ASCII-Zeichen");
    return;
  }
  // Andere Browser müssen sich mit dem neuen Passwort anmelden
  c.auth.revokeOthers(req);
  sendOk(res);
}

// ---------------------------------------------------------------------------
// /api/v1/login, /api/v1/logout  (öffentlich)
// ---------------------------------------------------------------------------
void login(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  const uint32_t nowMs = hal::now().millis();
  char token[Auth::kTokenHex + 1];
  const Auth::Result r = c.auth.login(doc["password"] | "", nowMs, token);
  if (r == Auth::Result::Locked) {
    char s[8];
    snprintf(s, sizeof(s), "%lu", static_cast<unsigned long>(c.auth.lockedFor(nowMs) / 1000 + 1));
    res.header("Retry-After", s);
    sendError(res, 429, Err::Throttled, "Zu viele Fehlversuche - bitte 30 s warten");
    return;
  }
  if (r != Auth::Result::Ok) {
    sendError(res, 401, Err::Auth, "Passwort falsch");
    return;
  }
  char cookie[96];
  snprintf(cookie, sizeof(cookie), "%s=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=%lu",
           Auth::kCookie, token, static_cast<unsigned long>(Auth::kSessionIdleMs / 1000));
  res.header("Set-Cookie", cookie);
  sendOk(res);
}

void logout(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  c.auth.logout(req);
  char cookie[64];
  snprintf(cookie, sizeof(cookie), "%s=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0",
           Auth::kCookie);
  res.header("Set-Cookie", cookie);
  sendOk(res);
}

void secretWifi(WebContext& c, hal::Request& req, hal::Response& res, const char* rest) {
  const int slot = atoi(rest) - 1;
  if (slot < 0 || slot > 2 || rest[1] != '\0') {
    sendError(res, 404, Err::NotFound, "Slot 1 bis 3");
    return;
  }
  if (slot == c.net.activeSlot()) {
    sendError(res, 409, Err::Busy, "Der aktive WLAN-Slot ist nicht änderbar");
    return;
  }
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  const char* ssid = doc["ssid"] | "";
  const char* pw = doc["password"] | "";
  const bool clear = doc["clear"] | false;
  const bool keep = !clear && *pw == '\0';
  const Err e = c.cfg.setWifi(static_cast<uint8_t>(slot), ssid, clear ? "" : pw, keep);
  if (e != Err::None) {
    sendError(res, 400, e, "SSID bis 32, Passwort bis 64 Zeichen");
    return;
  }
  pushEvent(c, EventType::ConfigChanged, SecWifi, (doc["reconnect"] | false) ? 1 : 0);
  sendOk(res);
}

void secretMailKey(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  if (c.cfg.setMailKey(doc["key"] | "") != Err::None) {
    sendError(res, 400, Err::InvalidArgument, "Schlüssel muss 32 Byte Base64 sein");
    return;
  }
  sendOk(res);
}

// ---------------------------------------------------------------------------
// System
// ---------------------------------------------------------------------------
void reboot(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  sendOk(res);
  pushEvent(c, EventType::RebootRequested, 0);
}

void factoryReset(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  sendOk(res);
  pushEvent(c, EventType::FactoryResetRequested);
}

void logsGet(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  uint32_t since = 0;
  argU32(req, "since", since);
  beginJson(res);
  JsonWriter w(res);
  c.log.writeSince(w, since);
  res.end();
}

// ---------------------------------------------------------------------------
// /api/v1/logs/settings  GET | PATCH  (öffentlich wie die Logs selbst)
// Nur Log-Level und Debug-Schalter; Datei-Log und alles andere bleibt in
// /api/v1/config hinter dem Login.
// ---------------------------------------------------------------------------
namespace {
const char kLogLevelNames[4][8] PS_PROGMEM = {"DEBUG", "INFO", "WARNING", "ERROR"};
const char kLogPublicKeys[7][13] PS_PROGMEM = {
    "level", "debugRam", "debugCycle", "debugSensor", "debugDisplay", "debugWeb", "debugMail"};
} // namespace

void logsSettings(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  if (req.method() == hal::HttpMethod::Patch) {
    JsonDocument doc;
    if (!parseBody(req, res, doc))
      return;
    JsonObjectConst patch = doc.as<JsonObjectConst>();
    JsonObjectConst log = patch["log"].as<JsonObjectConst>();
    bool allowed = !log.isNull() && patch.size() == 1;
    for (JsonPairConst kv : log) {
      bool known = false;
      for (const auto& k : kLogPublicKeys)
        known = known || ps_strcmp_P(kv.key().c_str(), k) == 0;
      allowed = allowed && known;
    }
    if (!allowed) {
      sendError(res, 400, Err::InvalidArgument, "Nur Log-Level und Debug-Schalter");
      return;
    }
    schema::FieldErrors errs;
    uint8_t changed = 0;
    const Err e = c.cfg.patchDevice(patch, errs, changed);
    if (e != Err::None) {
      sendError(res, 400, e, "Ungültige Werte", &errs);
      return;
    }
    if (changed)
      pushEvent(c, EventType::ConfigChanged, changed);
  }
  const DeviceConfig& d = c.cfg.device();
  char level[8];
  ps_memcpy_P(level, kLogLevelNames[d.logLevel < 4 ? d.logLevel : 1], sizeof(level));
  beginJson(res);
  JsonWriter w(res);
  w.beginObject().kv(FS_("ok"), true);
  w.key(FS_("log")).beginObject();
  w.kv(FS_("level"), static_cast<const char*>(level));
  w.kv(FS_("debugRam"), d.debugRam);
  w.kv(FS_("debugCycle"), d.debugCycle);
  w.kv(FS_("debugSensor"), d.debugSensor);
  w.kv(FS_("debugDisplay"), d.debugDisplay);
  w.kv(FS_("debugWeb"), d.debugWeb);
  w.kv(FS_("debugMail"), d.debugMail);
  w.endObject().endObject();
  res.end();
}

void logsFile(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  hal::FileGuard f(c.fs, c.fs.open("/log.txt", hal::OpenMode::Read));
  if (!f) {
    sendError(res, 404, Err::NotFound, "Keine Logdatei vorhanden");
    return;
  }
  res.header("Content-Disposition", "attachment; filename=log.txt");
  res.begin(200, "text/plain; charset=utf-8", static_cast<int32_t>(f->size()));
  uint8_t buf[256];
  for (;;) {
    const size_t n = f->read(buf, sizeof(buf));
    if (!n)
      break;
    res.write(reinterpret_cast<const char*>(buf), n);
  }
  res.end();
}

// ---------------------------------------------------------------------------
// Sensoren
// ---------------------------------------------------------------------------
void sensorsGet(WebContext& c, hal::Request& req, hal::Response& res, const char* rest) {
  if (req.method() == hal::HttpMethod::Patch) {
    sensorsPatch(c, req, res, rest);
    return;
  }
  const Instant now = hal::now();
  const SensorsConfig& sc = c.cfg.sensors();
  beginJson(res);
  JsonWriter w(res);
  w.beginObject();
  w.kv(FS_("ok"), true);
  w.kv(FS_("now"), now.millis());
  w.kv(FS_("flowerChannel"), c.cfg.device().flowerChannel.c_str());
  w.key(FS_("led")).beginObject();
  w.kv(FS_("available"), board::kFeatures.ledTraffic);
  w.kv(FS_("mode"), static_cast<uint32_t>(c.cfg.device().ledMode));
  w.kv(FS_("channel"), c.cfg.device().ledChannel.c_str());
  w.kv(FS_("onlyRed"), c.cfg.device().ledOnlyRed);
  w.kv(FS_("displayAvailable"), board::kFeatures.display);
  w.endObject();
  w.key(FS_("sensors")).beginArray();
  for (uint8_t i = 0; i < board::kSensorCount; i++) {
    const auto& se = board::kSensors[i];
    const SensorRunState& run = c.sensors.run(i);
    char id[16];
    copyP(id, sizeof(id), catalog::id(se.type));
    w.beginObject();
    w.kv(FS_("id"), static_cast<const char*>(id));
    schema::writeAll(w, schema::sensorTable(), &sc.sensors[i]);
    w.kv(FS_("health"), healthName(run.health));
    w.kv(FS_("failures"), static_cast<uint32_t>(run.failures));
    w.kv(FS_("active"), run.active);
    w.kv(FS_("warmupMs"), c.sensors.inWarmup(i, now)
                              ? static_cast<uint32_t>((run.startWarmupUntil - now).millis())
                              : 0u);
    w.kv(FS_("measured"), c.sensors.measuredOnce(i));
    w.kv(FS_("lastMeasurement"), c.sensors.lastMeasurement(i).millis());
    w.kv(FS_("tare"), se.type == SensorType::Hx711);
    w.key(FS_("channels")).beginArray();
    for (uint8_t ch = 0; ch < se.channels; ch++) {
      const uint8_t slot = static_cast<uint8_t>(se.firstSlot + ch);
      const ChannelDef d = catalog::channel(se.type, ch);
      const ChannelState& st = c.sensors.channel(slot);
      char key[32];
      schema::keyOfSlot(slot, key, sizeof(key));
      w.beginObject();
      w.kv(FS_("key"), static_cast<const char*>(key));
      w.kv(FS_("index"), static_cast<uint32_t>(ch));
      schema::writeAll(w, schema::channelTable(), &sc.channels[slot]);
      w.kv(FS_("oneSided"), d.oneSided);
      w.kv(FS_("hasRaw"), d.hasRaw);
      w.key(FS_("validMin")).value(d.validMin, 1);
      w.key(FS_("validMax")).value(d.validMax, 1);
      w.key(FS_("value")).value(st.value, 2);
      w.key(FS_("status")).value(statusName(c.sensors.status(slot, now)));
      w.key(FS_("obsMin")).value(st.obsMin, 2);
      w.key(FS_("obsMax")).value(st.obsMax, 2);
      if (d.hasRaw) {
        w.kv(FS_("raw"), static_cast<int32_t>(st.raw));
        if (st.hasRawObs()) {
          w.kv(FS_("rawObsMin"), static_cast<int32_t>(st.rawMin));
          w.kv(FS_("rawObsMax"), static_cast<int32_t>(st.rawMax));
        } else {
          w.key(FS_("rawObsMin")).null();
          w.key(FS_("rawObsMax")).null();
        }
      }
      w.endObject();
    }
    w.endArray();
    w.endObject();
  }
  w.endArray();
  w.endObject();
  res.end();
}

void sensorsPatch(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  schema::FieldErrors errs;
  uint32_t changed = 0;
  const Err e = c.cfg.patchSensors(doc.as<JsonObjectConst>(), errs, changed);
  if (e != Err::None) {
    sendError(res, 400, e, "Ungültige Werte", &errs);
    return;
  }
  if (changed) {
    c.sensors.applyConfig(changed, hal::now());
    pushEvent(c, EventType::SensorConfigChanged, 0xFF, changed);
  }
  beginJson(res);
  JsonWriter w(res);
  w.beginObject().kv(FS_("ok"), true).kv(FS_("changed"), changed).endObject();
  res.end();
}

/// POST /api/v1/sensors/<ID>/measure | /<ID>/<idx>/reset-extrema | /HX711/tare
void sensorsAction(WebContext& c, hal::Request& req, hal::Response& res, const char* rest) {
  char id[24];
  const char* slash = strchr(rest, '/');
  if (!slash || static_cast<size_t>(slash - rest) >= sizeof(id)) {
    sendError(res, 404, Err::NotFound);
    return;
  }
  memcpy(id, rest, static_cast<size_t>(slash - rest));
  id[slash - rest] = '\0';
  const int sensor = SensorService::sensorByName(id);
  if (sensor < 0) {
    sendError(res, 404, Err::NotFound, "Sensor nicht gefunden");
    return;
  }
  const char* action = slash + 1;
  const Instant now = hal::now();
  if (strcmp(action, "measure") == 0) {
    uint32_t wait = 0;
    const MeasureResult r = c.sensors.measureNow(static_cast<uint8_t>(sensor), now, false, wait);
    if (r == MeasureResult::Accepted) {
      sendOk(res);
    } else if (r == MeasureResult::NotReady) {
      char msg[48];
      snprintf(msg, sizeof(msg), "Sensor wärmt noch %lu s auf",
               static_cast<unsigned long>(wait / 1000));
      sendError(res, 409, Err::NotReady, msg);
    } else {
      sendError(res, 409, r == MeasureResult::Busy ? Err::Busy : Err::Disabled);
    }
    return;
  }
  if (strcmp(action, "tare") == 0) {
    const Err e = c.sensors.tare(static_cast<uint8_t>(sensor), now);
    if (e != Err::None)
      sendError(res, statusFor(e), e);
    else
      sendOk(res);
    return;
  }
  // <idx>/reset-extrema
  char* end = nullptr;
  const long idx = strtol(action, &end, 10);
  if (end && strcmp(end, "/reset-extrema") == 0 && idx >= 0 &&
      idx < board::kSensors[sensor].channels) {
    bool raw = false;
    if (req.body() && req.bodyLength()) {
      JsonDocument doc;
      if (!parseBody(req, res, doc))
        return;
      raw = doc["raw"] | false;
    }
    c.sensors.resetExtrema(static_cast<uint8_t>(board::kSensors[sensor].firstSlot + idx), raw, now);
    sendOk(res);
    return;
  }
  sendError(res, 404, Err::NotFound);
}

// ---------------------------------------------------------------------------
// Mail
// ---------------------------------------------------------------------------
void mailTest(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  if (!c.cfg.device().mailEnabled) {
    sendError(res, 409, Err::Disabled, "Mailversand ist ausgeschaltet");
    return;
  }
  c.mail.requestTest(hal::now());
  sendOk(res);
}

void mailStatus(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  beginJson(res);
  JsonWriter w(res);
  c.mail.writeStatus(w);
  res.end();
}

void mailTemplates(WebContext& c, hal::Request& req, hal::Response& res, const char* rest) {
  TemplateKind k;
  if (!TemplateStore::kindFromName(rest, k)) {
    sendError(res, 404, Err::NotFound, "boot, warning, alive oder style");
    return;
  }
  if (req.method() == hal::HttpMethod::Get) {
    beginJson(res);
    JsonWriter w(res);
    c.templates.writeJson(w, k);
    res.end();
    return;
  }
  if (req.method() == hal::HttpMethod::Delete) {
    const Err e = c.templates.reset(k);
    if (e != Err::None)
      sendError(res, 500, e);
    else
      sendOk(res);
    return;
  }
  JsonDocument doc;
  if (!parseBody(req, res, doc))
    return;
  schema::FieldErrors errs;
  const Err e = c.templates.save(k, doc["subject"] | "", doc["body"] | "", errs);
  if (e != Err::None) {
    sendError(res, statusFor(e), e, "Vorlage nicht gespeichert", &errs);
    return;
  }
  sendOk(res);
}

// ---------------------------------------------------------------------------
// OTA
// ---------------------------------------------------------------------------
void otaDone(WebContext& c, hal::Request& req, hal::Response& res, const char* rest) {
  if (req.method() == hal::HttpMethod::Get) {
    otaStatus(c, req, res, rest);
    return;
  }
  if (!c.ota.succeeded()) {
    if (c.ota.active())
      c.ota.abort("Upload unvollständig");
    sendError(res, 400, Err::Io, c.ota.error()[0] ? c.ota.error() : "Keine Firmware empfangen");
    return;
  }
  beginJson(res);
  res.write("{\"ok\":true,\"success\":true,\"needsReboot\":true}");
  res.end();
  pushEvent(c, EventType::RebootRequested, 3);
}

void otaStatus(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  beginJson(res);
  JsonWriter w(res);
  c.ota.writeStatus(w);
  res.end();
}

} // namespace web
} // namespace ps
