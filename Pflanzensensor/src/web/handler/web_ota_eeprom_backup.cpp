/**
 * @file web_ota_eeprom_backup.cpp
 * @brief Implementation of EEPROM-based backup for OTA updates
 */

#include "web_ota_eeprom_backup.h"
#include "managers/manager_config.h"
#include "managers/manager_config_preferences.h"
#include "logger/logger.h"
#include <EEPROM.h>
#include <Preferences.h>

extern ConfigManager ConfigMgr;

bool EEPROMBackup::begin() {
  EEPROM.begin(EEPROM_SIZE);
  logger.info(F("EEPROMBackup"), F("EEPROM initialisiert (" + String(EEPROM_SIZE) + F(" Bytes)")));
  return true;
}

void EEPROMBackup::end() {
  EEPROM.end();
}

bool EEPROMBackup::hasValidBackup() {
  EEPROM.begin(EEPROM_SIZE);
  
  EEPROMBackupHeader header;
  EEPROM.get(EEPROM_HEADER_OFFSET, header);
  
  bool valid = (header.magic == EEPROM_MAGIC && header.version == EEPROM_VERSION);
  
  EEPROM.end();
  
  return valid;
}

void EEPROMBackup::clearBackup() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Write invalid magic number
  EEPROMBackupHeader header = {0};
  EEPROM.put(EEPROM_HEADER_OFFSET, header);
  EEPROM.commit();
  
  EEPROM.end();
  
  logger.info(F("EEPROMBackup"), F("EEPROM-Backup gelöscht"));
}

bool EEPROMBackup::backupGeneralSettings() {
  EEPROMGeneralSettings settings = {0};
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, true)) {
    logger.error(F("EEPROMBackup"), F("Fehler beim Lesen von General Settings"));
    return false;
  }
  
  String device_name = PreferencesManager::getString(prefs, "device_name", "Pflanzensensor");
  String admin_pwd = PreferencesManager::getString(prefs, "admin_pwd", "admin");
  String flower_sens = PreferencesManager::getString(prefs, "flower_sens", "");
  
  strncpy(settings.device_name, device_name.c_str(), sizeof(settings.device_name) - 1);
  strncpy(settings.admin_pwd, admin_pwd.c_str(), sizeof(settings.admin_pwd) - 1);
  strncpy(settings.flower_sens, flower_sens.c_str(), sizeof(settings.flower_sens) - 1);
  
  settings.md5_verify = PreferencesManager::getBool(prefs, "md5_verify", true);
  settings.collectd_en = PreferencesManager::getBool(prefs, "collectd_en", false);
  settings.file_log = PreferencesManager::getBool(prefs, "file_log", false);
  
  prefs.end();
  
  EEPROM.put(EEPROM_GENERAL_OFFSET, settings);
  
  logger.debug(F("EEPROMBackup"), F("General Settings gesichert"));
  return true;
}

bool EEPROMBackup::backupWiFiSettings() {
  EEPROMWiFiSettings settings = {0};
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::WIFI, true)) {
    logger.error(F("EEPROMBackup"), F("Fehler beim Lesen von WiFi Settings"));
    return false;
  }
  
  String ssid1 = PreferencesManager::getString(prefs, "ssid1", "");
  String pwd1 = PreferencesManager::getString(prefs, "pwd1", "");
  String ssid2 = PreferencesManager::getString(prefs, "ssid2", "");
  String pwd2 = PreferencesManager::getString(prefs, "pwd2", "");
  String ssid3 = PreferencesManager::getString(prefs, "ssid3", "");
  String pwd3 = PreferencesManager::getString(prefs, "pwd3", "");
  
  strncpy(settings.ssid1, ssid1.c_str(), sizeof(settings.ssid1) - 1);
  strncpy(settings.pwd1, pwd1.c_str(), sizeof(settings.pwd1) - 1);
  strncpy(settings.ssid2, ssid2.c_str(), sizeof(settings.ssid2) - 1);
  strncpy(settings.pwd2, pwd2.c_str(), sizeof(settings.pwd2) - 1);
  strncpy(settings.ssid3, ssid3.c_str(), sizeof(settings.ssid3) - 1);
  strncpy(settings.pwd3, pwd3.c_str(), sizeof(settings.pwd3) - 1);
  
  prefs.end();
  
  EEPROM.put(EEPROM_WIFI_OFFSET, settings);
  
  logger.debug(F("EEPROMBackup"), F("WiFi Settings gesichert"));
  return true;
}

bool EEPROMBackup::backupDisplaySettings() {
  EEPROMDisplaySettings settings = {0};
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DISP, true)) {
    logger.error(F("EEPROMBackup"), F("Fehler beim Lesen von Display Settings"));
    return false;
  }
  
  settings.show_ip = PreferencesManager::getBool(prefs, "show_ip", true);
  settings.show_clock = PreferencesManager::getBool(prefs, "show_clock", true);
  settings.show_flower = PreferencesManager::getBool(prefs, "show_flower", true);
  settings.show_fabmobil = PreferencesManager::getBool(prefs, "show_fabmobil", true);
  settings.screen_dur = PreferencesManager::getUInt(prefs, "screen_dur", 5000);
  
  String clock_fmt = PreferencesManager::getString(prefs, "clock_fmt", "24h");
  strncpy(settings.clock_fmt, clock_fmt.c_str(), sizeof(settings.clock_fmt) - 1);
  
  prefs.end();
  
  EEPROM.put(EEPROM_DISPLAY_OFFSET, settings);
  
  logger.debug(F("EEPROMBackup"), F("Display Settings gesichert"));
  return true;
}

bool EEPROMBackup::backupDebugSettings() {
  EEPROMDebugSettings settings = {0};
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::DEBUG, true)) {
    logger.error(F("EEPROMBackup"), F("Fehler beim Lesen von Debug Settings"));
    return false;
  }
  
  settings.ram = PreferencesManager::getBool(prefs, "ram", false);
  settings.meas_cycle = PreferencesManager::getBool(prefs, "meas_cycle", false);
  settings.sensor = PreferencesManager::getBool(prefs, "sensor", false);
  settings.display = PreferencesManager::getBool(prefs, "display", false);
  settings.websocket = PreferencesManager::getBool(prefs, "websocket", false);
  
  prefs.end();
  
  EEPROM.put(EEPROM_DEBUG_OFFSET, settings);
  
  logger.debug(F("EEPROMBackup"), F("Debug Settings gesichert"));
  return true;
}

bool EEPROMBackup::backupLogSettings() {
  EEPROMLogSettings settings = {0};
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LOG, true)) {
    logger.error(F("EEPROMBackup"), F("Fehler beim Lesen von Log Settings"));
    return false;
  }
  
  settings.level = PreferencesManager::getUChar(prefs, "level", 3);
  settings.file_enabled = PreferencesManager::getBool(prefs, "file_enabled", false);
  
  prefs.end();
  
  EEPROM.put(EEPROM_LOG_OFFSET, settings);
  
  logger.debug(F("EEPROMBackup"), F("Log Settings gesichert"));
  return true;
}

bool EEPROMBackup::backupLEDSettings() {
  EEPROMLEDSettings settings = {0};
  
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::LED_TRAFFIC, true)) {
    logger.error(F("EEPROMBackup"), F("Fehler beim Lesen von LED Settings"));
    return false;
  }
  
  settings.mode = PreferencesManager::getUChar(prefs, "mode", 0);
  String sel_meas = PreferencesManager::getString(prefs, "sel_meas", "");
  strncpy(settings.sel_meas, sel_meas.c_str(), sizeof(settings.sel_meas) - 1);
  
  prefs.end();
  
  EEPROM.put(EEPROM_LED_OFFSET, settings);
  
  logger.debug(F("EEPROMBackup"), F("LED Settings gesichert"));
  return true;
}

bool EEPROMBackup::backupSensorSettings() {
  const char* sensorIds[] = {"ANALOG", "DHT"};
  uint16_t offset = EEPROM_SENSORS_OFFSET;
  
  for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    const char* sensorId = sensorIds[i];
    String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
    
    Preferences prefs;
    if (!prefs.begin(ns.c_str(), true)) {
      // Sensor not initialized, skip
      logger.debug(F("EEPROMBackup"), String(F("Sensor ")) + sensorId + F(" nicht initialisiert, überspringe"));
      offset += SENSOR_DATA_SIZE;
      continue;
    }
    
    if (!prefs.getBool("initialized", false)) {
      prefs.end();
      logger.debug(F("EEPROMBackup"), String(F("Sensor ")) + sensorId + F(" nicht initialisiert, überspringe"));
      offset += SENSOR_DATA_SIZE;
      continue;
    }
    
    EEPROMSensorConfig sensorConfig = {0};
    sensorConfig.initialized = true;
    strncpy(sensorConfig.sensor_id, sensorId, sizeof(sensorConfig.sensor_id) - 1);
    
    String name = prefs.getString("name", "");
    strncpy(sensorConfig.name, name.c_str(), sizeof(sensorConfig.name) - 1);
    sensorConfig.meas_interval = prefs.getUInt("meas_int", 30000);
    sensorConfig.has_error = prefs.getBool("has_err", false);
    
    // Determine number of measurements based on sensor type
    uint8_t maxMeas = (String(sensorId) == "ANALOG") ? MAX_MEASUREMENTS_ANALOG : MAX_MEASUREMENTS_DHT;
    sensorConfig.num_measurements = 0;
    
    // Backup measurements
    for (uint8_t m = 0; m < maxMeas; m++) {
      String prefix = "m" + String(m) + "_";
      String nameKey = prefix + "nm";
      
      if (!prefs.isKey(nameKey.c_str())) {
        continue;  // Measurement doesn't exist
      }
      
      EEPROMMeasurementConfig& meas = sensorConfig.measurements[m];
      meas.enabled = prefs.getBool((prefix + "en").c_str(), true);
      
      String measName = prefs.getString((prefix + "nm").c_str(), "");
      String fieldName = prefs.getString((prefix + "fn").c_str(), "");
      String unit = prefs.getString((prefix + "un").c_str(), "");
      
      strncpy(meas.name, measName.c_str(), sizeof(meas.name) - 1);
      strncpy(meas.field_name, fieldName.c_str(), sizeof(meas.field_name) - 1);
      strncpy(meas.unit, unit.c_str(), sizeof(meas.unit) - 1);
      
      meas.min_value = prefs.getFloat((prefix + "min").c_str(), 0.0);
      meas.max_value = prefs.getFloat((prefix + "max").c_str(), 100.0);
      meas.yellow_low = prefs.getFloat((prefix + "yl").c_str(), 0.0);
      meas.green_low = prefs.getFloat((prefix + "gl").c_str(), 0.0);
      meas.green_high = prefs.getFloat((prefix + "gh").c_str(), 100.0);
      meas.yellow_high = prefs.getFloat((prefix + "yh").c_str(), 100.0);
      meas.inverted = prefs.getBool((prefix + "inv").c_str(), false);
      meas.calibration_mode = prefs.getBool((prefix + "cal").c_str(), false);
      meas.autocal_duration = prefs.getUInt((prefix + "acd").c_str(), 0);
      meas.raw_min = prefs.getInt((prefix + "rmin").c_str(), 0);
      meas.raw_max = prefs.getInt((prefix + "rmax").c_str(), 1023);
      
      sensorConfig.num_measurements++;
    }
    
    prefs.end();
    
    // Write sensor config to EEPROM
    EEPROM.put(offset, sensorConfig);
    offset += SENSOR_DATA_SIZE;
    
    logger.info(F("EEPROMBackup"), String(F("Sensor ")) + sensorId + F(" gesichert (") + 
                String(sensorConfig.num_measurements) + F(" Messungen)"));
  }
  
  return true;
}

bool EEPROMBackup::backupAllSettings() {
  logger.info(F("EEPROMBackup"), F("Starte vollständige EEPROM-Sicherung..."));
  
  if (!begin()) {
    return false;
  }
  
  bool success = true;
  success &= backupGeneralSettings();
  success &= backupWiFiSettings();
  success &= backupDisplaySettings();
  success &= backupDebugSettings();
  success &= backupLogSettings();
  success &= backupLEDSettings();
  success &= backupSensorSettings();
  
  if (success) {
    // Write valid header with checksum
    EEPROMBackupHeader header;
    header.magic = EEPROM_MAGIC;
    header.version = EEPROM_VERSION;
    header.flags = 0xFF;  // All settings backed up
    header.timestamp = millis();
    header.checksum = calculateChecksum();
    
    EEPROM.put(EEPROM_HEADER_OFFSET, header);
    EEPROM.commit();
    
    logger.info(F("EEPROMBackup"), F("EEPROM-Sicherung erfolgreich abgeschlossen"));
  } else {
    logger.error(F("EEPROMBackup"), F("EEPROM-Sicherung fehlgeschlagen"));
  }
  
  end();
  
  return success;
}

uint16_t EEPROMBackup::calculateChecksum() {
  uint16_t checksum = 0;
  // Simple checksum: XOR of all data bytes
  for (uint16_t i = sizeof(EEPROMBackupHeader); i < EEPROM_SIZE; i++) {
    checksum ^= EEPROM.read(i);
  }
  return checksum;
}

// Restore functions

bool EEPROMBackup::restoreGeneralSettings() {
  EEPROMGeneralSettings settings;
  EEPROM.get(EEPROM_GENERAL_OFFSET, settings);
  
  auto r1 = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "device_name", String(settings.device_name));
  auto r2 = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "admin_pwd", String(settings.admin_pwd));
  auto r3 = PreferencesManager::updateStringValue(PreferencesNamespaces::GENERAL, "flower_sens", String(settings.flower_sens));
  auto r4 = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "md5_verify", settings.md5_verify);
  auto r5 = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "collectd_en", settings.collectd_en);
  auto r6 = PreferencesManager::updateBoolValue(PreferencesNamespaces::GENERAL, "file_log", settings.file_log);
  
  bool success = r1.isSuccess() && r2.isSuccess() && r3.isSuccess() && 
                 r4.isSuccess() && r5.isSuccess() && r6.isSuccess();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("General Settings wiederhergestellt"));
  }
  
  return success;
}

bool EEPROMBackup::restoreWiFiSettings() {
  EEPROMWiFiSettings settings;
  EEPROM.get(EEPROM_WIFI_OFFSET, settings);
  
  auto r1 = PreferencesManager::updateWiFiCredentials(1, String(settings.ssid1), String(settings.pwd1));
  auto r2 = PreferencesManager::updateWiFiCredentials(2, String(settings.ssid2), String(settings.pwd2));
  auto r3 = PreferencesManager::updateWiFiCredentials(3, String(settings.ssid3), String(settings.pwd3));
  
  bool success = r1.isSuccess() && r2.isSuccess() && r3.isSuccess();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("WiFi Settings wiederhergestellt"));
  }
  
  return success;
}

bool EEPROMBackup::restoreDisplaySettings() {
  EEPROMDisplaySettings settings;
  EEPROM.get(EEPROM_DISPLAY_OFFSET, settings);
  
  auto r1 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_ip", settings.show_ip);
  auto r2 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_clock", settings.show_clock);
  auto r3 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_flower", settings.show_flower);
  auto r4 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DISP, "show_fabmobil", settings.show_fabmobil);
  auto r5 = PreferencesManager::updateUIntValue(PreferencesNamespaces::DISP, "screen_dur", settings.screen_dur);
  auto r6 = PreferencesManager::updateStringValue(PreferencesNamespaces::DISP, "clock_fmt", String(settings.clock_fmt));
  
  bool success = r1.isSuccess() && r2.isSuccess() && r3.isSuccess() && 
                 r4.isSuccess() && r5.isSuccess() && r6.isSuccess();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("Display Settings wiederhergestellt"));
  }
  
  return success;
}

bool EEPROMBackup::restoreDebugSettings() {
  EEPROMDebugSettings settings;
  EEPROM.get(EEPROM_DEBUG_OFFSET, settings);
  
  auto r1 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "ram", settings.ram);
  auto r2 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "meas_cycle", settings.meas_cycle);
  auto r3 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "sensor", settings.sensor);
  auto r4 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "display", settings.display);
  auto r5 = PreferencesManager::updateBoolValue(PreferencesNamespaces::DEBUG, "websocket", settings.websocket);
  
  bool success = r1.isSuccess() && r2.isSuccess() && r3.isSuccess() && 
                 r4.isSuccess() && r5.isSuccess();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("Debug Settings wiederhergestellt"));
  }
  
  return success;
}

bool EEPROMBackup::restoreLogSettings() {
  EEPROMLogSettings settings;
  EEPROM.get(EEPROM_LOG_OFFSET, settings);
  
  auto r1 = PreferencesManager::updateUInt8Value(PreferencesNamespaces::LOG, "level", settings.level);
  auto r2 = PreferencesManager::updateBoolValue(PreferencesNamespaces::LOG, "file_enabled", settings.file_enabled);
  
  bool success = r1.isSuccess() && r2.isSuccess();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("Log Settings wiederhergestellt"));
  }
  
  return success;
}

bool EEPROMBackup::restoreLEDSettings() {
  EEPROMLEDSettings settings;
  EEPROM.get(EEPROM_LED_OFFSET, settings);
  
  auto r1 = PreferencesManager::updateUInt8Value(PreferencesNamespaces::LED_TRAFFIC, "mode", settings.mode);
  auto r2 = PreferencesManager::updateStringValue(PreferencesNamespaces::LED_TRAFFIC, "sel_meas", String(settings.sel_meas));
  
  bool success = r1.isSuccess() && r2.isSuccess();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("LED Settings wiederhergestellt"));
  }
  
  return success;
}

bool EEPROMBackup::restoreSensorSettings() {
  uint16_t offset = EEPROM_SENSORS_OFFSET;
  
  for (uint8_t i = 0; i < MAX_SENSORS; i++) {
    EEPROMSensorConfig sensorConfig;
    EEPROM.get(offset, sensorConfig);
    offset += SENSOR_DATA_SIZE;
    
    if (!sensorConfig.initialized) {
      logger.debug(F("EEPROMBackup"), String(F("Sensor Slot ")) + String(i) + F(" nicht initialisiert, überspringe"));
      continue;
    }
    
    String sensorId = String(sensorConfig.sensor_id);
    logger.info(F("EEPROMBackup"), String(F("Stelle Sensor ")) + sensorId + F(" wieder her..."));
    
    // Restore sensor base settings
    auto r1 = PreferencesManager::saveSensorSettings(sensorId, String(sensorConfig.name), 
                                                      sensorConfig.meas_interval, sensorConfig.has_error);
    
    if (!r1.isSuccess()) {
      logger.error(F("EEPROMBackup"), String(F("Fehler beim Wiederherstellen von Sensor ")) + sensorId);
      continue;
    }
    
    // Restore measurements
    for (uint8_t m = 0; m < sensorConfig.num_measurements && m < MAX_MEASUREMENTS_ANALOG; m++) {
      const EEPROMMeasurementConfig& meas = sensorConfig.measurements[m];
      
      auto r2 = PreferencesManager::saveSensorMeasurement(
        sensorId, m,
        meas.enabled,
        String(meas.name),
        String(meas.field_name),
        String(meas.unit),
        meas.min_value,
        meas.max_value,
        meas.yellow_low,
        meas.green_low,
        meas.green_high,
        meas.yellow_high,
        meas.inverted,
        meas.calibration_mode,
        meas.autocal_duration,
        meas.raw_min,
        meas.raw_max
      );
      
      if (!r2.isSuccess()) {
        logger.error(F("EEPROMBackup"), String(F("Fehler beim Wiederherstellen von Messung ")) + 
                     String(m) + F(" für Sensor ") + sensorId);
      }
    }
    
    logger.info(F("EEPROMBackup"), String(F("Sensor ")) + sensorId + F(" wiederhergestellt (") + 
                String(sensorConfig.num_measurements) + F(" Messungen)"));
  }
  
  return true;
}

bool EEPROMBackup::restoreAllSettings() {
  logger.info(F("EEPROMBackup"), F("Starte EEPROM-Wiederherstellung..."));
  
  if (!hasValidBackup()) {
    logger.error(F("EEPROMBackup"), F("Keine gültige EEPROM-Sicherung gefunden"));
    return false;
  }
  
  if (!begin()) {
    return false;
  }
  
  // Verify checksum
  EEPROMBackupHeader header;
  EEPROM.get(EEPROM_HEADER_OFFSET, header);
  uint16_t calcChecksum = calculateChecksum();
  
  if (header.checksum != calcChecksum) {
    logger.warning(F("EEPROMBackup"), F("Checksum-Warnung - Daten könnten beschädigt sein"));
    // Continue anyway - better than losing everything
  }
  
  bool success = true;
  success &= restoreGeneralSettings();
  success &= restoreWiFiSettings();
  success &= restoreDisplaySettings();
  success &= restoreDebugSettings();
  success &= restoreLogSettings();
  success &= restoreLEDSettings();
  success &= restoreSensorSettings();
  
  end();
  
  if (success) {
    logger.info(F("EEPROMBackup"), F("EEPROM-Wiederherstellung erfolgreich"));
  } else {
    logger.error(F("EEPROMBackup"), F("EEPROM-Wiederherstellung mit Fehlern abgeschlossen"));
  }
  
  return success;
}
