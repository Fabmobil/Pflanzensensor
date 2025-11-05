#include "managers/manager_display.h"

#if USE_DISPLAY

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <time.h>

#include "configs/config.h"
#include "display/display.h"
#include "display/display_images.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_led_traffic_light.h"
#include "managers/manager_sensor.h"
#include "utils/critical_section.h"
#include "utils/helper.h"
#include "utils/persistence_utils.h"
#include "utils/preferences_manager.h"
#include "utils/result_types.h"

extern std::unique_ptr<SensorManager> sensorManager;
#if USE_LED_TRAFFIC_LIGHT
extern std::unique_ptr<LedTrafficLightManager> ledTrafficLightManager;
#endif
extern Logger logger;

TypedResult<ResourceError, void> DisplayManager::initialize() {
#if USE_DISPLAY
  logger.debug(F("DisplayM"), F("Initialisiere DisplayManager"));

  m_display = std::make_unique<SSD1306Display>();
  if (!m_display) {
    return TypedResult<ResourceError, void>::fail(ResourceError::OPERATION_FAILED,
                                                  F("Display-Zuweisung fehlgeschlagen"));
  }
  auto displayResult = m_display->begin();
  if (!displayResult.isSuccess()) {
    return TypedResult<ResourceError, void>::fail(ResourceError::OPERATION_FAILED,
                                                  F("Display-Initialisierung fehlgeschlagen"));
  }

  auto loadResult = loadConfig();
  if (!loadResult.isSuccess()) {
    logger.warning(F("DisplayM"), F("Verwende Standard-Displaykonfiguration"));
    m_config.showIpScreen = true;
    m_config.showClock = true;
    m_config.showFlowerImage = true;
    m_config.showFabmobilImage = true;
    m_config.screenDuration = DISPLAY_DEFAULT_TIME * 1000;
    m_config.clockFormat = "24h";
    // Do not access sensorManager here!
  }

  logger.info(F("DisplayM"), F("DisplayManager erfolgreich initialisiert"));
  // Do not access sensorManager here!
  return TypedResult<ResourceError, void>::success();
#endif
  return TypedResult<ResourceError, void>::success();
}

/**
 * @brief Log enabled sensors and their IDs. Call this after both managers are
 * initialized.
 */
void DisplayManager::logEnabledSensors() {
#if USE_DISPLAY
  if (!sensorManager)
    return;
  String sensorCount =
      String(F("Anzahl aktivierter Sensoren: ")) + String(sensorManager->getSensors().size());
  logger.debug(F("DisplayM"), sensorCount);
  for (const auto& sensorPtr : sensorManager->getSensors()) {
    if (!sensorPtr || !sensorPtr->isEnabled())
      continue;
    String sensorId = sensorPtr->getId();
    String sensorMsg = String(F("Aktiver Sensor: ")) + sensorId;
    logger.debug(F("DisplayM"), sensorMsg);
  }
#endif
}

DisplayResult DisplayManager::loadConfig() {
#if USE_DISPLAY
  CriticalSection cs;

  // Check if Preferences exist, if not initialize with defaults
  if (!PreferencesManager::namespaceExists(PreferencesNamespaces::DISP)) {
    logger.info(F("DisplayM"), F("Keine Display-Konfiguration gefunden, initialisiere mit Standardwerten..."));
    auto initResult = PreferencesManager::initDisplayNamespace();
    if (!initResult.isSuccess()) {
      logger.warning(F("DisplayM"), F("Fehler beim Initialisieren der Display-Preferences"));
      // Use defaults from constructor
      return DisplayResult::success();
    }
  }

  // Load from Preferences
  logger.debug(F("DisplayM"), F("Lade Display-Konfiguration aus Preferences..."));
  
  auto result = PreferencesManager::loadDisplaySettings(
    m_config.showIpScreen, m_config.showClock,
    m_config.showFlowerImage, m_config.showFabmobilImage,
    m_config.screenDuration, m_config.clockFormat);
  
  if (result.isSuccess()) {
    logger.info(F("DisplayM"), F("Display-Konfiguration aus Preferences geladen"));
    
    String configMsg =
        String(F("Geladene Konfiguration - IP-Anzeige: ")) + String(m_config.showIpScreen) +
        String(F(", Uhr: ")) + String(m_config.showClock) + String(F(", Blume: ")) +
        String(m_config.showFlowerImage) + String(F(", Fabmobil: ")) +
        String(m_config.showFabmobilImage) + String(F(", Dauer: ")) +
        String(m_config.screenDuration) + String(F(", Format: ")) + m_config.clockFormat;
    logger.debug(F("DisplayM"), configMsg);
  } else {
    logger.warning(F("DisplayM"), F("Fehler beim Laden der Display-Konfiguration, verwende Standardwerte"));
  }

#endif
  return DisplayResult::success();
}

DisplayResult DisplayManager::validateConfig() {
  if (m_config.screenDuration < 1000 || m_config.screenDuration > 60000) {
    return DisplayResult::fail(DisplayError::INVALID_CONFIG,
                               F("Anzeigedauer muss zwischen 1 und 60 Sekunden liegen"));
  }

  if (m_config.clockFormat != "12h" && m_config.clockFormat != "24h") {
    return DisplayResult::fail(DisplayError::INVALID_CONFIG, F("Ungültiges Uhrzeitformat"));
  }

  return DisplayResult::success();
}

DisplayResult DisplayManager::saveConfig() {
#if USE_DISPLAY
  auto validation = validateConfig();
  if (!validation.isSuccess()) {
    return validation;
  }

  CriticalSection cs;

  // Save to Preferences
  logger.debug(F("DisplayM"), F("Speichere Display-Konfiguration in Preferences..."));
  
  auto result = PreferencesManager::saveDisplaySettings(
    m_config.showIpScreen, m_config.showClock,
    m_config.showFlowerImage, m_config.showFabmobilImage,
    m_config.screenDuration, m_config.clockFormat);
  
  if (!result.isSuccess()) {
    logger.error(F("DisplayM"), F("Fehler beim Speichern der Display-Konfiguration: ") + result.getMessage());
    return DisplayResult::fail(DisplayError::FILE_ERROR,
                               F("Speichern der Display-Konfiguration in Preferences fehlgeschlagen"));
  }

  logger.info(F("DisplayM"), F("Display-Konfiguration erfolgreich in Preferences gespeichert"));
  
  // Note: Sensor-specific display settings are kept in JSON for now
  // They can be migrated to a separate Preferences namespace in the future if needed
  if (!m_config.sensorDisplays.empty()) {
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.createNestedArray("sensor_displays");
    for (const auto& e : m_config.sensorDisplays) {
      JsonObject obj = arr.createNestedObject();
      obj["sensor_id"] = e.sensorId;
      JsonArray measurements = obj.createNestedArray("measurements");
      for (bool b : e.showMeasurements) {
        measurements.add(b);
      }
    }

    File configFile = LittleFS.open("/display_sensor_config.json", "w");
    if (configFile) {
      serializeJson(doc, configFile);
      configFile.close();
      logger.debug(F("DisplayM"), F("Sensor-spezifische Display-Einstellungen in JSON gespeichert"));
    }
  }
#endif
  return DisplayResult::success();
}

void DisplayManager::update() {
#if USE_DISPLAY
  unsigned long currentMillis = millis();

  if (currentMillis - m_lastScreenChange >= m_config.screenDuration) {
    rotateScreen();
    m_lastScreenChange = currentMillis;
  }
#endif
}

void DisplayManager::rotateScreen() {
#if USE_DISPLAY
  if (!m_display)
    return;
  if (!sensorManager) {
    logger.warning(F("DisplayM"), F("sensorManager is null in rotateScreen"));
    return;
  }

  unsigned long now = millis();
  if (now - m_lastScreenChange < m_config.screenDuration)
    return;

  m_lastScreenChange = now;

  // Count static screens (IP, clock, images)
  size_t staticScreens = 0;
  if (m_config.showIpScreen)
    staticScreens++;
  if (m_config.showClock)
    staticScreens++;
  if (m_config.showFlowerImage)
    staticScreens++;
  if (m_config.showFabmobilImage)
    staticScreens++;

  // Count measurement screens
  size_t measurementScreens = 0;
  for (const auto& sensorPtr : sensorManager->getSensors()) {
    if (!sensorPtr || !sensorPtr->isEnabled())
      continue;
    const SensorConfig& config = sensorPtr->config();
    for (size_t i = 0; i < config.activeMeasurements; ++i) {
      // A measurement is considered for display rotation only when the
      // sensor's measurement is enabled AND the display configuration
      // permits showing this measurement (display-only flag).
      if (config.measurements[i].enabled && isSensorMeasurementShown(sensorPtr->getId(), i)) {
        measurementScreens++;
      }
    }
  }
  size_t totalScreens = staticScreens + measurementScreens;

  size_t currentIndex = m_currentScreenIndex;

  // Show static screens
  if (m_config.showIpScreen && currentIndex == 0) {
    if (ConfigMgr.isDebugDisplay()) {
      logger.debug(F("DisplayM"), F("IP-Anzeige wird angezeigt"));
    }
    IPAddress ip;
    // Show softAP IP if we're in AP mode (manual AP started on failure)
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
      ip = WiFi.softAPIP();
    } else if (WiFi.status() == WL_CONNECTED) {
      ip = WiFi.localIP();
    } else {
      ip = IPAddress(0, 0, 0, 0);
    }
    String ipStr = (ip[0] == 0) ? F("(IP nicht gesetzt)") : ip.toString();
    m_display->showInfoScreen(ipStr);
    if (ledTrafficLightManager) {
      ledTrafficLightManager->handleDisplayUpdate();
    }
    m_currentScreenIndex++;
    if (m_currentScreenIndex >= totalScreens)
      m_currentScreenIndex = 0;
    return;
  }
  size_t idx = 0;
  if (m_config.showIpScreen)
    idx++;
  if (m_config.showClock && currentIndex == idx) {
    if (logger.isNTPInitialized()) {
      if (ConfigMgr.isDebugDisplay()) {
        logger.debug(F("DisplayM"), F("Uhr-Anzeige wird gezeigt"));
      }
      showClock();
    }
    if (ledTrafficLightManager) {
      ledTrafficLightManager->handleDisplayUpdate();
    }
    m_currentScreenIndex++;
    if (m_currentScreenIndex >= totalScreens)
      m_currentScreenIndex = 0;
    return;
  }
  if (m_config.showClock)
    idx++;
  if (m_config.showFlowerImage && currentIndex == idx) {
    if (ConfigMgr.isDebugDisplay()) {
      logger.debug(F("DisplayM"), F("Blumenbild wird gezeigt"));
    }
    showImage(displayImageFlower);
    if (ledTrafficLightManager) {
      ledTrafficLightManager->handleDisplayUpdate();
    }
    m_currentScreenIndex++;
    if (m_currentScreenIndex >= totalScreens)
      m_currentScreenIndex = 0;
    return;
  }
  if (m_config.showFlowerImage)
    idx++;
  if (m_config.showFabmobilImage && currentIndex == idx) {
    if (ConfigMgr.isDebugDisplay()) {
      logger.debug(F("DisplayM"), F("Fabmobil-Bild wird gezeigt"));
    }
    showImage(displayImageFabmobil);
    if (ledTrafficLightManager) {
      ledTrafficLightManager->handleDisplayUpdate();
    }
    m_currentScreenIndex++;
    if (m_currentScreenIndex >= totalScreens)
      m_currentScreenIndex = 0;
    return;
  }
  if (m_config.showFabmobilImage)
    idx++;

  // Show measurement screen (only call getSensor for the one to display)
  if (currentIndex >= staticScreens && (currentIndex - staticScreens) < measurementScreens) {
    size_t measurementIdx = currentIndex - staticScreens;
    size_t currentMeasurementIdx = 0;
    for (const auto& sensorPtr : sensorManager->getSensors()) {
      if (!sensorPtr || !sensorPtr->isEnabled())
        continue;
      const SensorConfig& config = sensorPtr->config();
      for (size_t i = 0; i < config.activeMeasurements; ++i) {
        if (config.measurements[i].enabled && isSensorMeasurementShown(sensorPtr->getId(), i)) {
          if (currentMeasurementIdx == measurementIdx) {
            if (ConfigMgr.isDebugDisplay()) {
              logger.debug(F("DisplayM"), String(F("Zeige Messung ")) + sensorPtr->getId() +
                                              String(F(":")) + String(i));
            }
            showSensorData(sensorPtr->getId(), i);
            m_currentScreenIndex++;
            if (m_currentScreenIndex >= totalScreens)
              m_currentScreenIndex = 0;
            return;
          }
          currentMeasurementIdx++;
        }
      }
    }
    m_currentScreenIndex++;
    if (m_currentScreenIndex >= totalScreens)
      m_currentScreenIndex = 0;
    return;
  }

  // Increment screen index and wrap around if needed (fallback)
  m_currentScreenIndex++;
  if (m_currentScreenIndex >= totalScreens) {
    m_currentScreenIndex = 0;
  }
#endif
}

void DisplayManager::showImage(const unsigned char* image) {
#if USE_DISPLAY
  if (m_display) {
    m_display->showBitmap(image);
  }
#endif
}

void DisplayManager::showSensorData(const String& sensorId, size_t measurementIndex) {
#if USE_DISPLAY
  if (!sensorManager) {
    logger.warning(F("DisplayM"), F("sensorManager ist null in showSensorData"));
    return;
  }
  if (auto sensor = sensorManager->getSensor(sensorId)) {
    auto measurementData = sensor->getMeasurementData();
    if (measurementData.isValid()) {
      // Clamp activeValues
      if (measurementData.activeValues > SensorConfig::MAX_MEASUREMENTS) {
        logger.warning(F("DisplayM"), F("Clamping activeValues from ") +
                                          String(measurementData.activeValues) + F(" to ") +
                                          String(SensorConfig::MAX_MEASUREMENTS));
      }
      size_t safeActiveValues =
          std::min(measurementData.activeValues, SensorConfig::MAX_MEASUREMENTS);
      // Check if this measurement index is valid
      if (measurementIndex < safeActiveValues && measurementIndex < measurementData.values.size()) {
        // Always use the user-friendly measurement name
        String measurementName = sensor->getMeasurementName(measurementIndex);
        if (measurementName.isEmpty()) {
          // fallback to field name if not set
          measurementName = measurementData.fieldNames[measurementIndex];
        }

        String sensorMsg = String(F("Zeige Sensor ")) + sensorId + String(F(" Messung ")) +
                           String(measurementIndex) + String(F(": name=")) + measurementName +
                           String(F(", Wert=")) + String(measurementData.values[measurementIndex]) +
                           String(F(", Einheit=")) + measurementData.units[measurementIndex];
        if (ConfigMgr.isDebugDisplay()) {
          logger.debug(F("DisplayM"), sensorMsg);
        }

        m_display->showMeasurementValue(measurementName, measurementData.values[measurementIndex],
                                        measurementData.units[measurementIndex]);

        // Update sensor status and control LED traffic light
        sensor->updateStatus(measurementIndex);

        if (ledTrafficLightManager) {
          uint8_t mode = ConfigMgr.getLedTrafficLightMode();

          if (mode == 0) {
            // Mode 0: LED traffic light off
            ledTrafficLightManager->turnOffAllLeds();
          } else if (mode == 1) {
            // Mode 1: Show status only when this measurement is displayed
            ledTrafficLightManager->setStatus(sensor->getStatus(measurementIndex));
          } else if (mode == 2) {
            // Mode 2: Always show status for selected measurement
            String measurementId = sensor->getId() + "_" + String(measurementIndex);
            ledTrafficLightManager->setMeasurementStatus(measurementId,
                                                         sensor->getStatus(measurementIndex));
          }
        }

        if (ConfigMgr.isDebugDisplay()) {
          logger.debug(F("DisplayM"),
                       "Sensor status: " + sensor->getStatus(measurementIndex) +
                           " für Wert: " + String(measurementData.values[measurementIndex]));
        }
      } else {
        String warningMsg = String(F("Ungültiger Messindex ")) + String(measurementIndex) +
                            String(F(" für Sensor ")) + sensorId;
        logger.warning(F("DisplayM"), warningMsg);
      }
    } else {
      String warningMsg = String(F("Ungültige Messdaten für Sensor ")) + sensorId;
      logger.warning(F("DisplayM"), warningMsg);
    }
  } else {
    String warningMsg = String(F("Sensor nicht gefunden: ")) + sensorId;
    logger.warning(F("DisplayM"), warningMsg);
  }
#endif
}

DisplayResult DisplayManager::setScreenDuration(unsigned long duration) {
#if USE_DISPLAY
  if (duration < 1000 || duration > 60000) {
    return DisplayResult::fail(DisplayError::INVALID_CONFIG, "Invalid screen duration");
  }

  m_config.screenDuration = duration;
  
  // Use unified config manager method
  auto result = ConfigMgr.setConfigValue("display", "screen_dur", String(duration), ConfigValueType::UINT);
  if (!result.isSuccess()) {
    return DisplayResult::fail(DisplayError::SAVE_FAILED, "Failed to save screen duration");
  }
  
  return DisplayResult::success();
#else
  return DisplayResult::success();
#endif
}

DisplayResult DisplayManager::setClockFormat(const String& format) {
#if USE_DISPLAY
  if (format != "12h" && format != "24h") {
    return DisplayResult::fail(DisplayError::INVALID_CONFIG, "Invalid clock format");
  }

  m_config.clockFormat = format;
  
  // Use unified config manager method
  auto result = ConfigMgr.setConfigValue("display", "clock_fmt", format, ConfigValueType::STRING);
  if (!result.isSuccess()) {
    return DisplayResult::fail(DisplayError::SAVE_FAILED, "Failed to save clock format");
  }
  
  return DisplayResult::success();
#else
  return DisplayResult::success();
#endif
}

DisplayResult DisplayManager::setClockEnabled(bool enabled) {
#if USE_DISPLAY
  m_config.showClock = enabled;
  
  // Use unified config manager method
  auto result = ConfigMgr.setConfigValue("display", "show_clock", enabled ? "true" : "false", ConfigValueType::BOOL);
  if (!result.isSuccess()) {
    return DisplayResult::fail(DisplayError::SAVE_FAILED, "Failed to save clock enabled");
  }
  
  return DisplayResult::success();
#else
  return DisplayResult::success();
#endif
}

DisplayResult DisplayManager::setIpScreenEnabled(bool enabled) {
#if USE_DISPLAY
  m_config.showIpScreen = enabled;
  
  // Use unified config manager method
  auto result = ConfigMgr.setConfigValue("display", "show_ip", enabled ? "true" : "false", ConfigValueType::BOOL);
  if (!result.isSuccess()) {
    return DisplayResult::fail(DisplayError::SAVE_FAILED, "Failed to save IP screen enabled");
  }
  
  return DisplayResult::success();
#else
  return DisplayResult::success();
#endif
}

DisplayResult DisplayManager::setFlowerImageEnabled(bool enabled) {
#if USE_DISPLAY
  m_config.showFlowerImage = enabled;
  
  // Use unified config manager method
  auto result = ConfigMgr.setConfigValue("display", "show_flower", enabled ? "true" : "false", ConfigValueType::BOOL);
  if (!result.isSuccess()) {
    return DisplayResult::fail(DisplayError::SAVE_FAILED, "Failed to save flower image enabled");
  }
  
  return DisplayResult::success();
#else
  return DisplayResult::success();
#endif
}

DisplayResult DisplayManager::setFabmobilImageEnabled(bool enabled) {
#if USE_DISPLAY
  m_config.showFabmobilImage = enabled;
  
  // Use unified config manager method
  auto result = ConfigMgr.setConfigValue("display", "show_fabmobil", enabled ? "true" : "false", ConfigValueType::BOOL);
  if (!result.isSuccess()) {
    return DisplayResult::fail(DisplayError::SAVE_FAILED, "Failed to save fabmobil image enabled");
  }
  
  return DisplayResult::success();
#else
  return DisplayResult::success();
#endif
}

DisplayResult DisplayManager::setSensorMeasurementDisplay(const String& sensorId,
                                                          size_t measurementIndex, bool enabled) {
#if USE_DISPLAY
  // Find or create entry for sensorId
  for (auto& entry : m_config.sensorDisplays) {
    if (entry.sensorId == sensorId) {
      if (measurementIndex >= entry.showMeasurements.size()) {
        // Resize to accommodate index
        entry.showMeasurements.resize(measurementIndex + 1, true);
      }
      entry.showMeasurements[measurementIndex] = enabled;
      return saveConfig();
    }
  }

  // Not found: create new entry. Default unknown measurements to true, then set
  DisplayConfig::SensorDisplayEntry newEntry;
  newEntry.sensorId = sensorId;
  newEntry.showMeasurements.resize(measurementIndex + 1, true);
  newEntry.showMeasurements[measurementIndex] = enabled;
  m_config.sensorDisplays.push_back(newEntry);
  return saveConfig();
#else
  return DisplayResult::success();
#endif
}

bool DisplayManager::isSensorMeasurementShown(const String& sensorId,
                                              size_t measurementIndex) const {
#if USE_DISPLAY
  // Search for explicit entry
  for (const auto& entry : m_config.sensorDisplays) {
    if (entry.sensorId == sensorId) {
      if (measurementIndex < entry.showMeasurements.size())
        return entry.showMeasurements[measurementIndex];
      // If index not present, default to true
      return true;
    }
  }
  // No explicit entry: caller should fall back to sensor's own config
  return true;
#else
  return true;
#endif
}

void DisplayManager::showInfoScreen(const String& ipAddress) {
#if USE_DISPLAY
  if (m_display) {
    m_display->showInfoScreen(ipAddress);
  }
#endif
}

void DisplayManager::showClock() {
#if USE_DISPLAY
  if (!m_display)
    return;

  String dateStr = Helper::getFormattedDate();
  String timeStr = Helper::getFormattedTime(m_config.clockFormat == "24h");

  if (dateStr == "Time not synced" || timeStr == "Time not synced") {
    logger.warning(F("DisplayM"), F("NTP nicht initialisiert, Uhr kann nicht angezeigt werden"));
    return;
  }

  m_display->showClock(dateStr, timeStr);

  if (ConfigMgr.isDebugDisplay()) {
    logger.debug(F("DisplayM"), F("Zeige Uhr: ") + dateStr + " " + timeStr);
  }
#endif
}

void DisplayManager::addLogLine(const String& status, bool isBootMode) {
#if USE_DISPLAY
  if (!m_bootMode && !m_updateMode)
    return;

  // Add new status line, autoscroll if full
  if (m_logLineCount < BOOT_LOG_LINES) {
    m_logLines[m_logLineCount++] = status;
  } else {
    // Shift lines up (autoscroll)
    for (size_t i = 1; i < BOOT_LOG_LINES; ++i) {
      m_logLines[i - 1] = m_logLines[i];
    }
    m_logLines[BOOT_LOG_LINES - 1] = status;
  }

  if (m_display) {
    String header;
    if (isBootMode) {
      header = ConfigMgr.getDeviceName() + F(" startet:");
    } else {
      header = ConfigMgr.getDeviceName() + F(" Update:");
    }

    // Convert to std::vector<String> for display API
    std::vector<String> lines(m_logLines, m_logLines + m_logLineCount);
    m_display->showBootScreen(header, lines);
  }
#endif
}

void DisplayManager::showLogScreen(const String& status, bool isBootMode) {
#if USE_DISPLAY
  if (isBootMode) {
    m_bootMode = true;
    m_updateMode = false;
  } else {
    m_updateMode = true;
    m_bootMode = false;
  }

  m_logLineCount = 0;
  m_logLines[m_logLineCount++] = status;

  if (m_display) {
    String header;
    if (isBootMode) {
      header = ConfigMgr.getDeviceName() + F(" startet:");
    } else {
      header = ConfigMgr.getDeviceName() + F(" Update:");
    }

    std::vector<String> lines(m_logLines, m_logLines + m_logLineCount);
    m_display->showBootScreen(header, lines);
  }
#endif
}

void DisplayManager::updateLogStatus(const String& status, bool isBootMode) {
  addLogLine(status, isBootMode);
}

#endif // USE_DISPLAY
