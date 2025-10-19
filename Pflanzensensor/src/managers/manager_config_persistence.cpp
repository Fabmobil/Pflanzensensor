/**
 * @file manager_config_persistence.cpp
 * @brief Implementation of ConfigPersistence class
 */

#include "manager_config_persistence.h"

#include <ArduinoJson.h>
using namespace ArduinoJson;

#include <LittleFS.h>

#include "../logger/logger.h"
#include "../utils/critical_section.h"
#include "../utils/persistence_utils.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

bool ConfigPersistence::configFileExists() {
  return LittleFS.exists("/config.json");
}

size_t ConfigPersistence::getConfigFileSize() {
  if (!configFileExists()) {
    return 0;
  }

  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    return 0;
  }

  size_t size = f.size();
  f.close();
  return size;
}

// --- Thresholds persistence helpers ---
// Sensor includes moved to manager_sensor_persistence.cpp

// Sensor settings are now handled by SensorPersistence
// This function has been moved to manager_sensor_persistence.cpp

ConfigPersistence::PersistenceResult ConfigPersistence::loadFromFile(
    ConfigData& config) {
  logger.logMemoryStats(F("ConfigP_load_before"));
  String errorMsg;
  StaticJsonDocument<512> doc;
  doc.clear();

  if (!PersistenceUtils::fileExists("/config.json")) {
    // Datei existiert nicht: Standardwerte verwenden, nicht als Fehler behandeln
    logger.info(F("ConfigP"), F("Konfigurationsdatei nicht gefunden, verwende Standardwerte."));
    auto result = resetToDefaults(config);
    logger.logMemoryStats(F("ConfigP_load_after"));
    return result;
  }

  if (!PersistenceUtils::readJsonFile("/config.json", doc, errorMsg)) {
    logger.error(F("ConfigP"), F("Konfiguration konnte nicht geladen werden: ") + errorMsg);
    logger.logMemoryStats(F("ConfigP_load_after"));
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }

  // Only log file contents if there is a parse error (handled in utility)
  // No info log for successful load

  // Load main configuration values
  config.adminPassword = doc["admin_password"] | INITIAL_ADMIN_PASSWORD;
  config.md5Verification = doc["md5_verification"] | false;
  config.fileLoggingEnabled =
      doc["file_logging_enabled"] | FILE_LOGGING_ENABLED;
  config.deviceName = doc["device_name"] | String(DEVICE_NAME);

  // Load WiFi credentials
  config.wifiSSID1 = doc["wifi_ssid_1"] | "";
  config.wifiPassword1 = doc["wifi_password_1"] | "";
  config.wifiSSID2 = doc["wifi_ssid_2"] | "";
  config.wifiPassword2 = doc["wifi_password_2"] | "";
  config.wifiSSID3 = doc["wifi_ssid_3"] | "";
  config.wifiPassword3 = doc["wifi_password_3"] | "";

  // Load debug flags - use runtime values, fallback to compile-time defaults
  // only if not present
  config.debugRAM = doc.containsKey("debug_ram") ? doc["debug_ram"] : DEBUG_RAM;
  config.debugMeasurementCycle = doc.containsKey("debug_measurement_cycle")
                                     ? doc["debug_measurement_cycle"]
                                     : DEBUG_MEASUREMENT_CYCLE;
  config.debugSensor =
      doc.containsKey("debug_sensor") ? doc["debug_sensor"] : DEBUG_SENSOR;
  config.debugDisplay =
      doc.containsKey("debug_display") ? doc["debug_display"] : DEBUG_DISPLAY;
  config.debugWebSocket = doc.containsKey("debug_websocket")
                              ? doc["debug_websocket"]
                              : DEBUG_WEBSOCKET;

  // LED Traffic Light settings
  config.ledTrafficLightMode = doc.containsKey("led_traffic_light_mode")
                                   ? doc["led_traffic_light_mode"]
                                   : 1;  // Default to mode 1 (all measurements)
  config.ledTrafficLightSelectedMeasurement =
      doc.containsKey("led_traffic_light_selected_measurement")
          ? doc["led_traffic_light_selected_measurement"].as<String>()
          : "";  // Default to empty (no measurement selected)

  // Flower Status settings
  config.flowerStatusSensor =
      doc.containsKey("flower_status_sensor")
          ? doc["flower_status_sensor"].as<String>()
          : "ANALOG_1";  // Default to ANALOG_1 (Bodenfeuchte)

#if USE_MAIL
  loadMailConfig(doc, config);
#endif

  logger.logMemoryStats(F("ConfigP_load_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::resetToDefaults(
    ConfigData& config) {
  logger.logMemoryStats(F("ConfigP_reset_before"));
  StaticJsonDocument<512> doc;
  doc.clear();

  config.adminPassword = INITIAL_ADMIN_PASSWORD;
  config.md5Verification = false;
  config.fileLoggingEnabled = FILE_LOGGING_ENABLED;
  config.deviceName = String(DEVICE_NAME);

  // Initialize debug flags to defaults
  config.debugRAM = DEBUG_RAM;
  config.debugMeasurementCycle = DEBUG_MEASUREMENT_CYCLE;
  config.debugSensor = DEBUG_SENSOR;
  config.debugDisplay = DEBUG_DISPLAY;
  config.debugWebSocket = DEBUG_WEBSOCKET;

  // LED Traffic Light settings - default to mode 1
  config.ledTrafficLightMode = 1;
  config.ledTrafficLightSelectedMeasurement = "";

  // Flower Status settings - default to ANALOG_1 (Bodenfeuchte)
  config.flowerStatusSensor = "ANALOG_1";

#if USE_MAIL
  setMailConfigDefaults(config);
#endif

  doc["admin_password"] = config.adminPassword;
  doc["md5_verification"] = config.md5Verification;
  doc["collectd_enabled"] = config.collectdEnabled;
  doc["file_logging_enabled"] = config.fileLoggingEnabled;
  doc["device_name"] = config.deviceName;
  doc["debug_ram"] = config.debugRAM;
  doc["debug_measurement_cycle"] = config.debugMeasurementCycle;
  doc["debug_sensor"] = config.debugSensor;
  doc["debug_display"] = config.debugDisplay;
  doc["debug_websocket"] = config.debugWebSocket;
  doc["wifi_ssid_1"] = config.wifiSSID1;
  doc["wifi_password_1"] = config.wifiPassword1;
  doc["wifi_ssid_2"] = config.wifiSSID2;
  doc["wifi_password_2"] = config.wifiPassword2;
  doc["wifi_ssid_3"] = config.wifiSSID3;
  doc["wifi_password_3"] = config.wifiPassword3;
  doc["led_traffic_light_mode"] = config.ledTrafficLightMode;
  doc["led_traffic_light_selected_measurement"] =
      config.ledTrafficLightSelectedMeasurement;
#if USE_MAIL
  doc["mail_enabled"] = config.mailEnabled;
  doc["smtp_host"] = config.smtpHost;
  doc["smtp_port"] = config.smtpPort;
  doc["smtp_user"] = config.smtpUser;
  doc["smtp_password"] = config.smtpPassword;
  doc["smtp_sender_name"] = config.smtpSenderName;
  doc["smtp_sender_email"] = config.smtpSenderEmail;
  doc["smtp_recipient"] = config.smtpRecipient;
  doc["smtp_enable_starttls"] = config.smtpEnableStartTLS;
  doc["smtp_debug"] = config.smtpDebug;
  doc["smtp_send_test_mail_on_boot"] = config.smtpSendTestMailOnBoot;
#endif

  String errorMsg;
  if (PersistenceUtils::writeJsonFile("/config.json", doc, errorMsg)) {
    logger.info(F("ConfigP"), F("Minimale Konfigurationsdatei erstellt"));
  } else {
    logger.error(F("ConfigP"),
                 F("Erstellen der initialen Konfigurationsdatei fehlgeschlagen: ") + errorMsg);
  }

  logger.logMemoryStats(F("ConfigP_reset_after"));
  return PersistenceResult::success();
}

ConfigPersistence::PersistenceResult ConfigPersistence::saveToFileMinimal(
    const ConfigData& config) {
  StaticJsonDocument<512> doc;
  doc[F("admin_password")] = config.adminPassword;
  doc[F("md5_verification")] = config.md5Verification;
  doc[F("collectd_enabled")] = config.collectdEnabled;
  doc[F("file_logging_enabled")] = config.fileLoggingEnabled;
  doc[F("device_name")] = config.deviceName;
  doc[F("debug_ram")] = config.debugRAM;
  doc[F("debug_measurement_cycle")] = config.debugMeasurementCycle;
  doc[F("debug_sensor")] = config.debugSensor;
  doc[F("debug_display")] = config.debugDisplay;
  doc[F("debug_websocket")] = config.debugWebSocket;
  doc[F("wifi_ssid_1")] = config.wifiSSID1;
  doc[F("wifi_password_1")] = config.wifiPassword1;
  doc[F("wifi_ssid_2")] = config.wifiSSID2;
  doc[F("wifi_password_2")] = config.wifiPassword2;
  doc[F("wifi_ssid_3")] = config.wifiSSID3;
  doc[F("wifi_password_3")] = config.wifiPassword3;
  doc[F("led_traffic_light_mode")] = config.ledTrafficLightMode;
  doc[F("led_traffic_light_selected_measurement")] = config.ledTrafficLightSelectedMeasurement;
  doc[F("flower_status_sensor")] = config.flowerStatusSensor;

#if USE_MAIL
  saveMailConfigToJson(doc, config);
#endif

  String errorMsg;
  if (!PersistenceUtils::writeJsonFile("/config.json", doc, errorMsg)) {
    return PersistenceResult::fail(ConfigError::FILE_ERROR, errorMsg);
  }
  return PersistenceResult::success();
}

void ConfigPersistence::writeUpdateFlagsToFile(bool fs, bool fw) {
  File f = LittleFS.open("/update_flags.txt", "w");
  if (f) {
    f.printf("fs:%d,fw:%d\n", fs ? 1 : 0, fw ? 1 : 0);
    f.close();
  }
}

void ConfigPersistence::readUpdateFlagsFromFile(bool& fs, bool& fw) {
  File f = LittleFS.open("/update_flags.txt", "r");
  if (f) {
    String line = f.readStringUntil('\n');
    fs = line.indexOf("fs:1") != -1;
    fw = line.indexOf("fw:1") != -1;
    f.close();
  } else {
    fs = false;
    fw = false;
  }
}

#if USE_MAIL
// Helper function to load mail configuration
void ConfigPersistence::loadMailConfig(const StaticJsonDocument<512>& doc, ConfigData& config) {
  // Default for mail_enabled: if compile-time SMTP_HOST is provided enable by
  // default (convenience for preconfigured builds), otherwise keep disabled
  // for security.
#ifdef SMTP_HOST
  config.mailEnabled = doc.containsKey(F("mail_enabled")) ? doc[F("mail_enabled")] : true;
#else
  config.mailEnabled = doc.containsKey(F("mail_enabled")) ? doc[F("mail_enabled")] : false;
#endif

  config.smtpHost = doc.containsKey(F("smtp_host")) ? doc[F("smtp_host")].as<String>()
#ifdef SMTP_HOST
                    : F(SMTP_HOST);
#else
                    : F("");
#endif

  config.smtpPort = doc.containsKey(F("smtp_port")) ? doc[F("smtp_port")]
#ifdef SMTP_PORT
                    : SMTP_PORT;
#else
                    : 587;
#endif

  config.smtpUser = doc.containsKey(F("smtp_user")) ? doc[F("smtp_user")].as<String>()
#ifdef SMTP_USER
                    : F(SMTP_USER);
#else
                    : F("");
#endif

  config.smtpPassword = doc.containsKey(F("smtp_password")) ? doc[F("smtp_password")].as<String>()
#ifdef SMTP_PASSWORD
                        : F(SMTP_PASSWORD);
#else
                        : F("");
#endif

  config.smtpSenderName = doc.containsKey(F("smtp_sender_name")) ? doc[F("smtp_sender_name")].as<String>()
#ifdef SMTP_SENDER_NAME
                          : F(SMTP_SENDER_NAME);
#else
                          : F(DEVICE_NAME);
#endif

  config.smtpSenderEmail = doc.containsKey(F("smtp_sender_email")) ? doc[F("smtp_sender_email")].as<String>()
#ifdef SMTP_SENDER_EMAIL
                           : F(SMTP_SENDER_EMAIL);
#else
                           : F("");
#endif

  config.smtpRecipient = doc.containsKey(F("smtp_recipient")) ? doc[F("smtp_recipient")].as<String>()
#ifdef SMTP_RECIPIENT
                         : F(SMTP_RECIPIENT);
#else
                         : F("");
#endif

  config.smtpEnableStartTLS = doc.containsKey(F("smtp_enable_starttls")) ? doc[F("smtp_enable_starttls")]
#ifdef SMTP_ENABLE_STARTTLS
                              : SMTP_ENABLE_STARTTLS;
#else
                              : true;
#endif

  config.smtpDebug = doc.containsKey(F("smtp_debug")) ? doc[F("smtp_debug")]
#ifdef SMTP_DEBUG
                     : SMTP_DEBUG;
#else
                     : false;
#endif

  config.smtpSendTestMailOnBoot = doc.containsKey(F("smtp_send_test_mail_on_boot")) ? doc[F("smtp_send_test_mail_on_boot")]
#ifdef SMTP_SEND_TEST_MAIL_ON_BOOT
                                  : SMTP_SEND_TEST_MAIL_ON_BOOT;
#else
                                  : false;
#endif
}

// Helper function to set mail configuration defaults
void ConfigPersistence::setMailConfigDefaults(ConfigData& config) {
  // Default mail enabled: enable by default only when compile-time SMTP_HOST
  // is provided (useful for images preconfigured to send mail). Otherwise
  // keep it disabled for security.
#ifdef SMTP_HOST
  config.mailEnabled = true;
#else
  config.mailEnabled = false;  // Disabled by default for security
#endif

#ifdef SMTP_HOST
  config.smtpHost = F(SMTP_HOST);
#else
  config.smtpHost = F("");
#endif

#ifdef SMTP_PORT
  config.smtpPort = SMTP_PORT;
#else
  config.smtpPort = 587;  // Standard SMTP port
#endif

#ifdef SMTP_USER
  config.smtpUser = F(SMTP_USER);
#else
  config.smtpUser = F("");
#endif

#ifdef SMTP_PASSWORD
  config.smtpPassword = F(SMTP_PASSWORD);
#else
  config.smtpPassword = F("");
#endif

#ifdef SMTP_SENDER_NAME
  config.smtpSenderName = F(SMTP_SENDER_NAME);
#else
  config.smtpSenderName = F(DEVICE_NAME);
#endif

#ifdef SMTP_SENDER_EMAIL
  config.smtpSenderEmail = F(SMTP_SENDER_EMAIL);
#else
  config.smtpSenderEmail = F("");
#endif

#ifdef SMTP_RECIPIENT
  config.smtpRecipient = F(SMTP_RECIPIENT);
#else
  config.smtpRecipient = F("");
#endif

#ifdef SMTP_ENABLE_STARTTLS
  config.smtpEnableStartTLS = SMTP_ENABLE_STARTTLS;
#else
  config.smtpEnableStartTLS = true;  // Enable STARTTLS by default
#endif

#ifdef SMTP_DEBUG
  config.smtpDebug = SMTP_DEBUG;
#else
  config.smtpDebug = false;
#endif

#ifdef SMTP_SEND_TEST_MAIL_ON_BOOT
  config.smtpSendTestMailOnBoot = SMTP_SEND_TEST_MAIL_ON_BOOT;
#else
  config.smtpSendTestMailOnBoot = false;
#endif
}

// Helper function to save mail configuration to JSON
void ConfigPersistence::saveMailConfigToJson(StaticJsonDocument<512>& doc, const ConfigData& config) {
  doc[F("mail_enabled")] = config.mailEnabled;
  doc[F("smtp_host")] = config.smtpHost;
  doc[F("smtp_port")] = config.smtpPort;
  doc[F("smtp_user")] = config.smtpUser;
  doc[F("smtp_password")] = config.smtpPassword;
  doc[F("smtp_sender_name")] = config.smtpSenderName;
  doc[F("smtp_sender_email")] = config.smtpSenderEmail;
  doc[F("smtp_recipient")] = config.smtpRecipient;
  doc[F("smtp_enable_starttls")] = config.smtpEnableStartTLS;
  doc[F("smtp_debug")] = config.smtpDebug;
  doc[F("smtp_send_test_mail_on_boot")] = config.smtpSendTestMailOnBoot;
}
#endif // USE_MAIL
