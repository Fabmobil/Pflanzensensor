/**
 * @file config_service.h
 * @brief Konfiguration laden, validieren, ändern, gedrosselt speichern,
 *        exportieren, importieren, zurücksetzen
 * @details Dateien (refactor.md 5.7): /device.cfg, /sensors.cfg, /secrets.cfg.
 *          Gespeichert wird nur aus persist.tick, mit 2 s Debounce.
 */
#pragma once

#include "core/config/defaults.h"
#include "core/config/document.h"
#include "core/config/legacy_import.h"
#include "hal/fs.h"
#include "services/state_store.h"
#include "services/template_store.h"

namespace ps {

class ConfigService {
public:
  static constexpr const char* kDevicePath = "/device.cfg";
  static constexpr const char* kSensorsPath = "/sensors.cfg";
  static constexpr const char* kSecretsPath = "/secrets.cfg";
  static constexpr const char* kImportPath = "/import.tmp";
  static constexpr uint32_t kDebounceMs = 2000;

  ConfigService(hal::Fs& fs, TemplateStore& templates, StateStore& state)
      : fs_(fs), templates_(templates), state_(state) {}

  /// Standardwerte setzen und Dateien lesen
  void load();

  const DeviceConfig& device() const { return dev_; }
  const SensorsConfig& sensors() const { return sens_; }
  const Secrets& secrets() const { return sec_; }

  /**
   * @brief Teil-JSON auf die Gerätekonfiguration anwenden (atomar)
   * @param changed Bitmaske ConfigSection
   */
  Err patchDevice(JsonObjectConst patch, schema::FieldErrors& errs, uint8_t& changed);
  /// Teil-JSON auf Sensoren/Kanäle anwenden (atomar)
  Err patchSensors(JsonObjectConst patch, schema::FieldErrors& errs, uint32_t& changedSensors);

  /// Direkter Zugriff für Dienste (z.B. Autokalibrierung, Tara); markiert speichern
  SensorsConfig& sensorsMutable() { return sens_; }
  void markSensorsDirty(Instant now);
  void markDeviceDirty(Instant now);

  Err setWifi(uint8_t slot, const char* ssid, const char* password, bool keepPassword);
  Err setAdminPassword(const char* pw);
  Err setMailKey(const char* key);
  /// Passwort prüfen (eingestelltes bzw. Build-Standard)
  bool checkAdminPassword(const char* pw) const;
  static bool adminPasswordValid(const char* pw);

  /// Aus persist.tick
  void tick(Instant now, bool force);

  /// Export ohne Geheimnisse: {"schema":1,"device":…,"sensors":…,"templates":…}
  void exportTo(JsonWriter& w);

  struct ImportResult {
    legacy::Report report;
    bool legacyFormat = false;
    Err err = Err::None;
  };
  /// /import.tmp prüfen und übernehmen (schreibt sofort); Neustart macht der Aufrufer
  ImportResult importFile();
  /**
   * @brief Konfiguration übernehmen (Format der Firmware 2.x oder Export mit "schema": 1)
   * @details Schreibt sofort. Genutzt vom Upload (importFile) und vom
   *          Webflasher-Block beim Start (ProvisionService). Ergebnis über
   *          res (≈ 350 B), damit es nicht zweimal auf dem Stapel liegt.
   */
  void importJson(JsonObjectConst root, ImportResult& res);

  /// Konfiguration, Zustand und Vorlagen löschen (Chronik und Log bleiben)
  void factoryReset();

private:
  bool saveDevice();
  bool saveSensors();
  bool saveSecrets();
  void hashPassword(const char* pw, const uint8_t salt[16], uint8_t out[32]) const;

  hal::Fs& fs_;
  TemplateStore& templates_;
  StateStore& state_;
  DeviceConfig dev_;
  SensorsConfig sens_;
  Secrets sec_;
  bool devDirty_ = false, sensDirty_ = false, secDirty_ = false;
  Instant devSince_, sensSince_;
};

} // namespace ps
