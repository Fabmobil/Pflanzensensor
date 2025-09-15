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
  String section = _server.hasArg("section") ? _server.arg("section") : "";

  if (section == "debug") {
    // Debug RAM
    bool oldDebugRAM = ConfigMgr.isDebugRAM();
    bool newDebugRAM = _server.hasArg("debug_ram");
    if (oldDebugRAM != newDebugRAM) {
      auto result = ConfigMgr.setDebugRAM(newDebugRAM);
      if (result.isSuccess()) {
        changes += F("<li>Debug RAM ");
        changes += newDebugRAM ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
    // Debug Measurement Cycle
    bool oldDebugMeasurementCycle = ConfigMgr.isDebugMeasurementCycle();
    bool newDebugMeasurementCycle = _server.hasArg("debug_measurement_cycle");
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
    // Debug Sensor
    bool oldDebugSensor = ConfigMgr.isDebugSensor();
    bool newDebugSensor = _server.hasArg("debug_sensor");
    if (oldDebugSensor != newDebugSensor) {
      auto result = ConfigMgr.setDebugSensor(newDebugSensor);
      if (result.isSuccess()) {
        changes += F("<li>Debug Sensor ");
        changes += newDebugSensor ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
    // Debug Display
    bool oldDebugDisplay = ConfigMgr.isDebugDisplay();
    bool newDebugDisplay = _server.hasArg("debug_display");
    if (oldDebugDisplay != newDebugDisplay) {
      auto result = ConfigMgr.setDebugDisplay(newDebugDisplay);
      if (result.isSuccess()) {
        changes += F("<li>Debug Display ");
        changes += newDebugDisplay ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
    // Debug WebSocket
    bool oldDebugWebSocket = ConfigMgr.isDebugWebSocket();
    bool newDebugWebSocket = _server.hasArg("debug_websocket");
    if (oldDebugWebSocket != newDebugWebSocket) {
      auto result = ConfigMgr.setDebugWebSocket(newDebugWebSocket);
      if (result.isSuccess()) {
        changes += F("<li>Debug WebSocket ");
        changes += newDebugWebSocket ? F("aktiviert") : F("deaktiviert");
        changes += F("</li>");
        updated = true;
      }
    }
    // Log Level
    String oldLogLevel = ConfigMgr.getLogLevel();
    String newLogLevel =
        _server.hasArg("log_level") ? _server.arg("log_level") : oldLogLevel;
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
    bool newFileLogging = _server.hasArg("file_logging_enabled");
    if (oldFileLogging != newFileLogging) {
      ConfigMgr.setFileLoggingEnabled(newFileLogging);
      changes += F("<li>Datei-Logging ");
      changes += newFileLogging ? F("aktiviert") : F("deaktiviert");
      changes += F("</li>");
      updated = true;
    }
  } else if (section == "system") {
    // MD5 verification
    bool oldMD5 = ConfigMgr.isMD5Verification();
    bool newMD5 = _server.hasArg("md5_verification");
    if (oldMD5 != newMD5) {
      ConfigMgr.setMD5Verification(newMD5);
      changes += F("<li>MD5-Überprüfung ");
      changes += newMD5 ? F("aktiviert") : F("deaktiviert");
      changes += F("</li>");
      updated = true;
    }
    // Collectd enabled
    bool oldCollectd = ConfigMgr.isCollectdEnabled();
    bool newCollectd = _server.hasArg("collectd_enabled");
    if (oldCollectd != newCollectd) {
      ConfigMgr.setCollectdEnabled(newCollectd);
      changes += F("<li>InfluxDB/Collectd ");
      changes += newCollectd ? F("aktiviert") : F("deaktiviert");
      changes += F("</li>");
      updated = true;
    }
    // Device name
    if (_server.hasArg("device_name")) {
      String newName = _server.arg("device_name");
      if (newName != ConfigMgr.getDeviceName()) {
        ConfigMgr.setDeviceName(newName);
        updated = true;
        changes += F("<li>Gerätename geändert</li>");
      }
    }
  } else if (section == "led_traffic_light") {
    // LED Traffic Light Mode
    uint8_t oldMode = ConfigMgr.getLedTrafficLightMode();
    uint8_t newMode = _server.hasArg("led_traffic_light_mode")
                          ? _server.arg("led_traffic_light_mode").toInt()
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
    String newMeasurement = _server.hasArg("led_traffic_light_measurement")
                                ? _server.arg("led_traffic_light_measurement")
                                : "";
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
