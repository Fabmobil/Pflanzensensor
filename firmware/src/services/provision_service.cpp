/**
 * @file provision_service.cpp
 */
#include "services/provision_service.h"

#include <ArduinoJson.h>
#include <new>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "hal/system.h"
#include "services/config_service.h"
#include "services/log.h"

namespace ps {

void ProvisionService::loadHardware() {
  hal::TempBuffer sector(provision::kSectorSize);
  if (!sector || !hal::provisionRead(sector.data(), sector.size())) {
    read_ = provision::Result::BadLength;
    return;
  }
  // Der Block lebt bis applySettings() (beim Start ist Heap reichlich frei)
  void* mem = hal::tempAlloc(sizeof(provision::Block));
  if (!mem)
    return;
  block_ = new (mem) provision::Block();
  read_ = provision::parse(sector.data(), sector.size(), board::defaultHardware(), *block_);
  if (read_ != provision::Result::Ok) {
    if (read_ != provision::Result::Empty)
      LOGW(Mod::Boot, "Webflasher-Block unbrauchbar (%s) - Voreinstellung %s",
           provision::resultName(read_), board::name());
    block_->~Block();
    hal::tempFree(block_);
    block_ = nullptr;
    return;
  }
  bool useHardware = block_->hasHardware;
#if defined(PS_LOCAL_DEFAULTS)
  // Lokales Geräteprofil (boards/local/): der Build ist für genau dieses
  // Gerät gemacht, sein Profil gilt. Hardware aus einem früheren
  // Webflasher-Block bleibt unbeachtet (der Block selbst bleibt unverändert).
  if (useHardware) {
    LOGI(Mod::Boot, "Hardware aus dem Webflasher ignoriert - lokales Profil %s", board::name());
    useHardware = false;
  }
#endif
  if (useHardware) {
    problem_ = board::setHardware(block_->hardware);
    if (problem_.problem == HwProblem::None) {
      fromBlock_ = true;
      LOGI(Mod::Boot, "Hardware aus dem Webflasher: %s, %u Sensoren, %u Kanäle", board::name(),
           board::sensorCount(), board::slotCount());
    } else {
      LOGE(Mod::Boot, "Hardware aus dem Webflasher ungültig (%s, Pin %d) - Voreinstellung %s",
           hwProblemCode(problem_.problem), problem_.pin, board::name());
    }
  }
}

void ProvisionService::applyChannelName(ConfigService& cfg, const provision::ChannelName& cn) {
  const int slot = schema::slotOfKey(cn.key);
  if (slot < 0) {
    LOGW(Mod::Config, "Kanalname aus dem Webflasher: %s gibt es nicht", cn.key);
    return;
  }
  // {"<Sensor-ID>": {"channels": {"<Index>": {"name": "…"}}}}
  char id[20];
  snprintf(id, sizeof(id), "%s", cn.key);
  char* us = strrchr(id, '_');
  *us = '\0';
  JsonDocument doc;
  doc[id]["channels"][us + 1]["name"] = cn.name;
  // FieldErrors (≈ 340 B) nicht auf den Stack (Grenze 512 B je Rahmen)
  hal::TempBuffer mem(sizeof(schema::FieldErrors));
  if (!mem)
    return;
  schema::FieldErrors& errs = *new (mem.data()) schema::FieldErrors();
  uint32_t changed = 0;
  if (cfg.patchSensors(doc.as<JsonObjectConst>(), errs, changed) == Err::None)
    LOGI(Mod::Config, "Kanal %s heißt jetzt %s", cn.key, cn.name);
  else
    LOGW(Mod::Config, "Kanalname für %s ungültig", cn.key);
}

void ProvisionService::importConfig(ConfigService& cfg) {
  // Der Block steht nur noch im Flash (parse() hat "config" übersprungen)
  JsonDocument doc;
  provision::Result r;
  {
    hal::TempBuffer sector(provision::kSectorSize);
    if (!sector || !hal::provisionRead(sector.data(), sector.size())) {
      LOGE(Mod::Config, "Konfiguration aus dem Webflasher: Block nicht lesbar");
      return;
    }
    r = provision::readConfig(sector.data(), sector.size(), doc);
  }
  if (r != provision::Result::Ok) {
    LOGW(Mod::Config, "Konfiguration aus dem Webflasher unbrauchbar (%s)",
         provision::resultName(r));
    return;
  }
  // Ergebnis (≈ 350 B) nicht auf den Stapel (Grenze 512 B je Rahmen)
  hal::TempBuffer mem(sizeof(ConfigService::ImportResult));
  if (!mem)
    return;
  ConfigService::ImportResult& res = *new (mem.data()) ConfigService::ImportResult();
  cfg.importJson(doc["settings"]["config"], res);
  if (res.err != Err::None) {
    char why[32];
    copyP(why, sizeof(why), errText(res.err));
    LOGW(Mod::Config, "Konfiguration aus dem Webflasher nicht übernommen (%s)", why);
  } else {
    LOGI(Mod::Config,
         "Konfiguration aus dem Webflasher (%s): %u übernommen, %u ignoriert, %u ungültig",
         res.legacyFormat ? "Firmware 2.x" : "Firmware 3",
         static_cast<unsigned>(res.report.applied), static_cast<unsigned>(res.report.ignored),
         static_cast<unsigned>(res.report.invalid));
  }
  res.~ImportResult();
}

bool ProvisionService::applySettings(ConfigService& cfg) {
  if (!block_)
    return false;
  provision::Block& b = *block_;
  bool applied = false;
  if (b.settings.any()) {
    // Zuerst die importierte Datei, damit Name, WLAN, Kanalnamen und
    // Passwort aus dem Formular des Webflashers Vorrang haben.
    if (b.settings.hasConfig)
      importConfig(cfg);
    if (b.settings.hasDeviceName) {
      JsonDocument doc;
      doc["general"]["deviceName"] = b.settings.deviceName;
      schema::FieldErrors errs;
      uint8_t changed = 0;
      if (cfg.patchDevice(doc.as<JsonObjectConst>(), errs, changed) == Err::None)
        LOGI(Mod::Config, "Gerätename aus dem Webflasher: %s", b.settings.deviceName);
      else
        LOGW(Mod::Config, "Gerätename aus dem Webflasher ungültig");
    }
    for (uint8_t i = 0; i < provision::kWifiSlots; i++) {
      const provision::WifiSetting& w = b.settings.wifi[i];
      if (!w.set)
        continue;
      if (cfg.setWifi(i, w.ssid, w.password, false) == Err::None)
        LOGI(Mod::Wifi, "WLAN %u aus dem Webflasher: %s", i + 1, w.ssid);
      else
        LOGW(Mod::Wifi, "WLAN %u aus dem Webflasher ungültig", i + 1);
    }
    for (uint8_t i = 0; i < b.settings.channelNameCount; i++)
      applyChannelName(cfg, b.settings.channelNames[i]);
    if (b.settings.hasAdminPassword) {
      if (cfg.setAdminPassword(b.settings.adminPassword) == Err::None)
        LOGI(Mod::Auth, "Adminpasswort aus dem Webflasher gesetzt");
      else
        LOGW(Mod::Auth, "Adminpasswort aus dem Webflasher ungültig - nicht übernommen");
    }
    cfg.tick(hal::now(), true); // sofort speichern, bevor der Sektor bereinigt wird
    applied = true;
    // Block ohne Einstellungen zurückschreiben: keine Passwörter im Klartext
    // im Flash, und spätere Änderungen in der Oberfläche bleiben bestehen.
    hal::TempBuffer sector(provision::kSectorSize);
    if (sector && provision::encode(b, false, sector.data(), sector.size()) &&
        hal::provisionWrite(sector.data(), sector.size()))
      LOGI(Mod::Boot, "Einstellungen aus dem Webflasher übernommen");
    else
      LOGE(Mod::Boot, "Webflasher-Block konnte nicht bereinigt werden");
  }
  // Passwörter nicht im freigegebenen Speicher liegen lassen
  for (auto& w : b.settings.wifi)
    memset(w.password, 0, sizeof(w.password));
  memset(b.settings.adminPassword, 0, sizeof(b.settings.adminPassword));
  b.~Block();
  hal::tempFree(block_);
  block_ = nullptr;
  return applied;
}

} // namespace ps
