/**
 * @file admin_handler_utils.cpp
 * @brief Utility functions for admin handler
 * @details Provides helper functions for configuration processing, formatting,
 *          authentication, and configuration value handling
 */

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "web/handler/admin_handler.h"

String AdminHandler::formatMemorySize(size_t bytes) const {
  if (bytes < 1024) return String(bytes) + F(" B");
  if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + F(" KB");
  return String(bytes / 1024.0 / 1024.0, 1) + F(" MB");
}

String AdminHandler::formatUptime() const {
  unsigned long uptime = millis() / 1000;

  // Safety check for overflow
  if (uptime == 0 || uptime > 0xFFFFFFFF / 1000) {
    return F("Fehler");
  }

  unsigned int days = uptime / 86400;
  unsigned int hours = (uptime % 86400) / 3600;
  unsigned int minutes = (uptime % 3600) / 60;
  unsigned int seconds = uptime % 60;

  String formatted;
  if (days > 0) formatted += String(days) + F("d ");
  if (hours > 0) formatted += String(hours) + F("h ");
  formatted += String(minutes) + F("m ");
  formatted += String(seconds) + F("s");

  return formatted;
}

bool AdminHandler::validateRequest() const {
  if (!_server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())) {
    return false;
  }
  return true;
}

bool AdminHandler::processConfigUpdates(String& changes) {
  bool updated = false;

  // Detect JSON payloads and, if present, parse them into a JsonDocument so
  // callers can send application/json instead of form-encoded data. For
  // backwards compatibility, if parsing fails or the content-type is not
  // application/json, fall back to using the usual _server.arg/_server.hasArg
  // accessors.
  String contentType = _server.header("Content-Type");
  bool isJson = (contentType != "" && contentType.indexOf("application/json") != -1);
  DynamicJsonDocument doc(4096);
  if (isJson) {
    String body = _server.arg("plain");
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      logger.debug(F("AdminHandler"), "JSON-Parsing für Admin-Update fehlgeschlagen, fallback auf Formdaten");
      isJson = false;
    }
  }

  // Helper wrappers so the rest of the function can uniformly access args
  // regardless of whether they came from a JSON body or form-encoded data.
  auto serverHasArg = [&](const String& name) -> bool {
    if (isJson) return doc.containsKey(name.c_str());
    return _server.hasArg(name);
  };

  auto serverArg = [&](const String& name) -> String {
    if (isJson) {
      if (!doc.containsKey(name.c_str())) return String();
      JsonVariant v = doc[name.c_str()];
      if (v.is<const char*>()) return String(v.as<const char*>());
      if (v.is<bool>()) return v.as<bool>() ? String("true") : String("false");
      if (v.is<long>()) return String((long)v.as<long>());
      if (v.is<float>()) return String((float)v.as<float>());
      return String();
    }
    return _server.arg(name);
  };

  auto serverBool = [&](const String& name) -> bool {
    if (isJson) {
      if (!doc.containsKey(name.c_str())) return false;
      JsonVariant v = doc[name.c_str()];
      if (v.is<bool>()) return v.as<bool>();
      if (v.is<const char*>()) {
        String s = String(v.as<const char*>());
        return (s == "1" || s.equalsIgnoreCase("true"));
      }
      if (v.is<long>()) return v.as<long>() != 0;
      return false;
    }
    return _server.hasArg(name);
  };

  String section = serverHasArg("section") ? serverArg("section") : String();

  if (section == "debug") {
    // Debug RAM
    bool oldDebugRAM = ConfigMgr.isDebugRAM();
    if (serverHasArg("debug_ram")) {
      bool newDebugRAM = serverBool("debug_ram");
      if (oldDebugRAM != newDebugRAM) {
        auto result = ConfigMgr.setDebugRAM(newDebugRAM);
        if (result.isSuccess()) {
          changes += F("<li>Debug RAM ");
          changes += newDebugRAM ? F("aktiviert") : F("deaktiviert");
          changes += F("</li>");
          updated = true;
        }
      }
    }
    // Debug Measurement Cycle
    bool oldDebugMeasurementCycle = ConfigMgr.isDebugMeasurementCycle();
    if (serverHasArg("debug_measurement_cycle")) {
      bool newDebugMeasurementCycle = serverBool("debug_measurement_cycle");
      if (oldDebugMeasurementCycle != newDebugMeasurementCycle) {
        auto result =
            ConfigMgr.setDebugMeasurementCycle(newDebugMeasurementCycle);
        if (result.isSuccess()) {
          changes += F("<li>Debug Messzyklus ");
          changes += newDebugMeasurementCycle ? F("aktiviert") : F("deaktiviert");
          changes += F("</li>");
          updated = true;
        }
      }
    }
    // Debug Sensor
    bool oldDebugSensor = ConfigMgr.isDebugSensor();
    if (serverHasArg("debug_sensor")) {
      bool newDebugSensor = serverBool("debug_sensor");
      if (oldDebugSensor != newDebugSensor) {
        auto result = ConfigMgr.setDebugSensor(newDebugSensor);
        if (result.isSuccess()) {
          changes += F("<li>Debug Sensor ");
          changes += newDebugSensor ? F("aktiviert") : F("deaktiviert");
          changes += F("</li>");
          updated = true;
        }
      }
    }
    // Debug Display
    bool oldDebugDisplay = ConfigMgr.isDebugDisplay();
    if (serverHasArg("debug_display")) {
      bool newDebugDisplay = serverBool("debug_display");
      if (oldDebugDisplay != newDebugDisplay) {
        auto result = ConfigMgr.setDebugDisplay(newDebugDisplay);
        if (result.isSuccess()) {
          changes += F("<li>Debug Display ");
          changes += newDebugDisplay ? F("aktiviert") : F("deaktiviert");
          changes += F("</li>");
          updated = true;
        }
      }
    }
    // Debug WebSocket
    bool oldDebugWebSocket = ConfigMgr.isDebugWebSocket();
    if (serverHasArg("debug_websocket")) {
      bool newDebugWebSocket = serverBool("debug_websocket");
      if (oldDebugWebSocket != newDebugWebSocket) {
        auto result = ConfigMgr.setDebugWebSocket(newDebugWebSocket);
        if (result.isSuccess()) {
          changes += F("<li>Debug WebSocket ");
          changes += newDebugWebSocket ? F("aktiviert") : F("deaktiviert");
          changes += F("</li>");
          updated = true;
        }
      }
    }
    // Log Level
    String oldLogLevel = ConfigMgr.getLogLevel();
    String newLogLevel = serverHasArg("log_level") ? serverArg("log_level") : oldLogLevel;
    if (oldLogLevel != newLogLevel) {
      auto result = ConfigMgr.setLogLevel(newLogLevel);
      if (result.isSuccess()) {
        changes += F("<li>Log Level auf ");
        changes += newLogLevel;
        changes += F(" gesetzt</li>");
        updated = true;
      }
    }
    // File logging enabled
    bool oldFileLogging = ConfigMgr.isFileLoggingEnabled();
    if (serverHasArg("file_logging_enabled")) {
      bool newFileLogging = serverBool("file_logging_enabled");
      if (oldFileLogging != newFileLogging) {
        ConfigMgr.setFileLoggingEnabled(newFileLogging);
        changes += F("<li>Datei-Logging ");
        changes += newFileLogging ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
  } else if (section == "system") {
    // MD5 verification
    bool oldMD5 = ConfigMgr.isMD5Verification();
    if (serverHasArg("md5_verification")) {
      bool newMD5 = serverBool("md5_verification");
      if (oldMD5 != newMD5) {
        ConfigMgr.setMD5Verification(newMD5);
        changes += F("<li>MD5-Überprüfung ");
        changes += newMD5 ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
    // Collectd enabled
    bool oldCollectd = ConfigMgr.isCollectdEnabled();
    if (serverHasArg("collectd_enabled")) {
      bool newCollectd = serverBool("collectd_enabled");
      if (oldCollectd != newCollectd) {
        ConfigMgr.setCollectdEnabled(newCollectd);
        changes += F("<li>InfluxDB/Collectd ");
        changes += newCollectd ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
    // Device name
    if (serverHasArg("device_name")) {
      String newName = serverArg("device_name");
      if (newName != ConfigMgr.getDeviceName()) {
        ConfigMgr.setDeviceName(newName);
        updated = true;
        changes += F("<li>Gerätename geändert</li>");
      }
    }
  } else if (section == "led_traffic_light") {
    // LED Traffic Light Mode
    uint8_t oldMode = ConfigMgr.getLedTrafficLightMode();
    uint8_t newMode = serverHasArg("led_traffic_light_mode")
                          ? serverArg("led_traffic_light_mode").toInt()
                          : oldMode;
    if (oldMode != newMode) {
      auto result = ConfigMgr.setLedTrafficLightMode(newMode);
      if (result.isSuccess()) {
        String modeText;
        if (newMode == 0)
          modeText = F("Modus 0 (LED-Ampel aus)");
        else if (newMode == 1)
          modeText = F("Modus 1 (Alle Messungen)");
        else if (newMode == 2)
          modeText = F("Modus 2 (Einzelmessung)");
        else
          modeText = F("Unbekannter Modus");

        changes += F("<li>LED-Ampel Modus auf ");
        changes += modeText;
        changes += F(" gesetzt</li>");
        updated = true;
      }
    }

    // LED Traffic Light Selected Measurement
    String oldMeasurement = ConfigMgr.getLedTrafficLightSelectedMeasurement();
    String newMeasurement = serverHasArg("led_traffic_light_measurement")
                                ? serverArg("led_traffic_light_measurement")
                                : String();
    if (oldMeasurement != newMeasurement) {
      auto result =
          ConfigMgr.setLedTrafficLightSelectedMeasurement(newMeasurement);
      if (result.isSuccess()) {
        if (newMeasurement.isEmpty()) {
          changes += F("<li>LED-Ampel Messung zurückgesetzt</li>");
        } else {
          changes += F("<li>LED-Ampel Messung auf ");
          changes += newMeasurement;
          changes += F(" gesetzt</li>");
        }
        updated = true;
      }
    }
#if USE_MAIL
  } else if (section == "mail") {
    // Mail enabled
    bool oldMailEnabled = ConfigMgr.isMailEnabled();
    bool newMailEnabled = serverBool("mail_enabled");
    if (oldMailEnabled != newMailEnabled) {
      auto result = ConfigMgr.setMailEnabled(newMailEnabled);
      if (result.isSuccess()) {
        changes += F("<li>E-Mail-Funktionen ");
        changes += newMailEnabled ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }

    // SMTP Host
    if (serverHasArg("smtp_host")) {
      String newSmtpHost = serverArg("smtp_host");
      if (newSmtpHost != ConfigMgr.getSmtpHost()) {
        auto result = ConfigMgr.setSmtpHost(newSmtpHost);
        if (result.isSuccess()) {
          changes += F("<li>SMTP-Server geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP Port
    if (serverHasArg("smtp_port")) {
      uint16_t newSmtpPort = serverArg("smtp_port").toInt();
      if (newSmtpPort != ConfigMgr.getSmtpPort()) {
        auto result = ConfigMgr.setSmtpPort(newSmtpPort);
        if (result.isSuccess()) {
          changes += F("<li>SMTP-Port geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP User
    if (serverHasArg("smtp_user")) {
      String newSmtpUser = serverArg("smtp_user");
      if (newSmtpUser != ConfigMgr.getSmtpUser()) {
        auto result = ConfigMgr.setSmtpUser(newSmtpUser);
        if (result.isSuccess()) {
          changes += F("<li>SMTP-Benutzername geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP Password
    if (serverHasArg("smtp_password")) {
      String newSmtpPassword = serverArg("smtp_password");
      if (newSmtpPassword != ConfigMgr.getSmtpPassword()) {
        auto result = ConfigMgr.setSmtpPassword(newSmtpPassword);
        if (result.isSuccess()) {
          changes += F("<li>SMTP-Passwort geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP Sender Name
    if (serverHasArg("smtp_sender_name")) {
      String newSmtpSenderName = serverArg("smtp_sender_name");
      if (newSmtpSenderName != ConfigMgr.getSmtpSenderName()) {
        auto result = ConfigMgr.setSmtpSenderName(newSmtpSenderName);
        if (result.isSuccess()) {
          changes += F("<li>Absender-Name geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP Sender Email
    if (serverHasArg("smtp_sender_email")) {
      String newSmtpSenderEmail = serverArg("smtp_sender_email");
      if (newSmtpSenderEmail != ConfigMgr.getSmtpSenderEmail()) {
        auto result = ConfigMgr.setSmtpSenderEmail(newSmtpSenderEmail);
        if (result.isSuccess()) {
          changes += F("<li>Absender-E-Mail geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP Recipient
    if (_server.hasArg("smtp_recipient")) {
      String newSmtpRecipient = _server.arg("smtp_recipient");
      if (newSmtpRecipient != ConfigMgr.getSmtpRecipient()) {
        auto result = ConfigMgr.setSmtpRecipient(newSmtpRecipient);
        if (result.isSuccess()) {
          changes += F("<li>Standard-Empfänger geändert</li>");
          updated = true;
        }
      }
    }

    // SMTP STARTTLS
    bool oldSmtpEnableStartTLS = ConfigMgr.isSmtpEnableStartTLS();
    bool newSmtpEnableStartTLS = _server.hasArg("smtp_enable_starttls");
    if (oldSmtpEnableStartTLS != newSmtpEnableStartTLS) {
      auto result = ConfigMgr.setSmtpEnableStartTLS(newSmtpEnableStartTLS);
      if (result.isSuccess()) {
        changes += F("<li>STARTTLS-Verschlüsselung ");
        changes += newSmtpEnableStartTLS ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }

    // SMTP Debug
    bool oldSmtpDebug = ConfigMgr.isSmtpDebug();
    bool newSmtpDebug = _server.hasArg("smtp_debug");
    if (oldSmtpDebug != newSmtpDebug) {
      auto result = ConfigMgr.setSmtpDebug(newSmtpDebug);
      if (result.isSuccess()) {
        changes += F("<li>SMTP-Debug ");
        changes += newSmtpDebug ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }

    // SMTP Send Test Mail on Boot
    bool oldSmtpSendTestMailOnBoot = ConfigMgr.isSmtpSendTestMailOnBoot();
    bool newSmtpSendTestMailOnBoot = _server.hasArg("smtp_send_test_mail_on_boot");
    if (oldSmtpSendTestMailOnBoot != newSmtpSendTestMailOnBoot) {
      auto result = ConfigMgr.setSmtpSendTestMailOnBoot(newSmtpSendTestMailOnBoot);
      if (result.isSuccess()) {
        changes += F("<li>Test-Mail beim Start ");
        changes += newSmtpSendTestMailOnBoot ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
#endif
  }
  return updated;
}

bool AdminHandler::applyConfigValue(const String& key, const String& value) {
  bool changed = false;

  if (key == "debug_ram") {
    changed = ConfigMgr.setDebugRAM(value == "true").isSuccess();
  } else if (key == "debug_measurement_cycle") {
    changed = ConfigMgr.setDebugMeasurementCycle(value == "true").isSuccess();
  } else if (key == "debug_sensor") {
    changed = ConfigMgr.setDebugSensor(value == "true").isSuccess();
  } else if (key == "debug_display") {
    changed = ConfigMgr.setDebugDisplay(value == "true").isSuccess();
  } else if (key == "debug_websocket") {
    changed = ConfigMgr.setDebugWebSocket(value == "true").isSuccess();
  } else if (key == "log_level") {
    changed = ConfigMgr.setLogLevel(value).isSuccess();
  } else if (key == "md5_verification") {
    bool enabled = (value == "1" || value.equalsIgnoreCase("true"));
    changed = ConfigMgr.setMD5Verification(enabled).isSuccess();
  } else if (key == "collectd_enabled") {
    bool enabled = (value == "1" || value.equalsIgnoreCase("true"));
    changed = ConfigMgr.setCollectdEnabled(enabled).isSuccess();
  } else if (key == "file_logging_enabled") {
    bool enabled = (value == "1" || value.equalsIgnoreCase("true"));
    changed = ConfigMgr.setFileLoggingEnabled(enabled).isSuccess();
  } else if (key == "admin_password") {
    changed = ConfigMgr.setAdminPassword(value).isSuccess();
  } else if (key == "led_traffic_light_mode") {
    uint8_t mode = value.toInt();
    changed = ConfigMgr.setLedTrafficLightMode(mode).isSuccess();
  } else if (key == "led_traffic_light_selected_measurement") {
    changed =
        ConfigMgr.setLedTrafficLightSelectedMeasurement(value).isSuccess();
  }

  return changed;
}
