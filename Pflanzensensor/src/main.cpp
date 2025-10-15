/**
 * @file main.cpp
 * @brief Main program for ESP8266 based sensor system with improved resource
 * management
 */

// Arduino & ESP Core
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <Wire.h>

// System Components
#include "configs/config.h"
#include "utils/critical_section.h"
#include "utils/persistence_utils.h"
#include "utils/result_types.h"

// Manager Classes
#include "managers/manager_config.h"
#include "managers/manager_config_persistence.h"
#include "managers/manager_display.h"
#include "managers/manager_led_traffic_light.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

// Network & Services
#if USE_WIFI
#include <NTPClient.h>

#include "utils/wifi.h"
#endif


#if USE_WEBSERVER
#include <ESP8266WebServer.h>

#include "web/core/web_manager.h"
#include "web/handler/admin_handler.h"
#include "web/handler/log_handler.h"
#endif

#if USE_WEBSOCKET
#include <WebSocketsServer.h>

#include "web/services/websocket.h"
#endif

#if USE_MAIL
#include "mail/mail_manager.h"
#include "mail/mail_helper.h"
#endif

// helper methods
#include "configs/default_json_generator.h"
#include "utils/helper.h"

// Global objects
extern std::unique_ptr<SensorManager> sensorManager;

#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;
#endif

#if USE_LED_TRAFFIC_LIGHT
extern std::unique_ptr<LedTrafficLightManager> ledTrafficLightManager;
#endif

void setup() {
  // Initialize serial communication first
  Serial.begin(115200);
  delay(100);   // Give serial time to initialize
  delay(1000);  // Give sensors time to power up and stabilize

  logger.beginMemoryTracking(F("managers_init"));

  // Initialize filesystem
  if (!Helper::initializeComponent(F("filesystem"), []() -> ResourceResult {
        CriticalSection cs;
        if (!LittleFS.begin()) {
          logger.error(F("main"), F("Dateisystem konnte nicht eingehängt werden"));
          return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR,
                                      F("Dateisystem konnte nicht eingehängt werden"));
        }
        return ResourceResult::success();
      })) {
    return;
  }

#if USE_LED_TRAFFIC_LIGHT
  if (!Helper::initializeComponent(
          F("LED traffic light manager"), []() -> ResourceResult {
            ledTrafficLightManager = std::make_unique<LedTrafficLightManager>();
            auto result = ledTrafficLightManager->init();
            if (!result.isSuccess()) {
              logger.warning(
                  F("main"),
                  F("LED-Ampel-Manager Initialisierung fehlgeschlagen: ") +
                      result.getMessage());
            }
            return result;
          })) {
    return;
  }
#endif

#if USE_DISPLAY
    if (!Helper::initializeComponent(
          F("display manager"), []() -> ResourceResult {
            displayManager = std::make_unique<DisplayManager>();
            return displayManager->init();
          })) {
    // Note: Don't return here - display is optional
    logger.warning(F("main"),
                   F("Display-Manager Initialisierung fehlgeschlagen, fahre fort"));
  } else {
    displayManager->showLogScreen(F("Filesystem..."), true);
  }
#endif

  // Ensure config and sensors JSON files exist
  ensureConfigFilesExist();

  // Initialize configuration
  if (!Helper::initializeComponent(F("configuration"), []() -> ResourceResult {
        auto result = ConfigMgr.loadConfig();
        if (!result.isSuccess()) {
          logger.error(F("main"), F("Konfiguration konnte nicht geladen werden: ") +
                                      result.getMessage());
          return ResourceResult::fail(ResourceError::CONFIG_ERROR,
                                      result.getMessage());
        }
        return ResourceResult::success();
      })) {
#if USE_DISPLAY
    if (displayManager)
      displayManager->updateLogStatus(F("Config Fehler"), true);
#endif
    return;
  }
#if USE_DISPLAY
  if (displayManager) displayManager->updateLogStatus(F("Config..."), true);
#endif

  // increase reboot count
  Helper::incrementRebootCount();

  // **CRITICAL FIX: Check for update mode BEFORE initializing heavy managers**
  if (ConfigMgr.getDoFirmwareUpgrade()) {
  logger.info(F("main"),
     F("Firmware-Upgrade-Modus erkannt - wechsle in Minimalmodus"));

#if USE_DISPLAY
    // Inform user about update mode on display
      if (displayManager) {
      displayManager->showLogScreen(F("Firmware-Update-Modus"), false);
      displayManager->updateLogStatus(F("Starte Minimal-Setup..."), false);
    }
#endif

#if USE_DISPLAY
    // Setup WiFi for update mode with real-time display updates
    if (!Helper::initializeComponent(F("WiFi"), []() -> ResourceResult {
          // Simplified approach without lambda callback
          auto result = setupWiFi();
          if (!result.isSuccess()) {
            logger.error(F("main"), F("WiFi-Initialisierung fehlgeschlagen: ") +
                                        result.getMessage());
          }
          return result;
        })) {
      return;
    }
#else
    // Setup WiFi for update mode without display
    if (!Helper::initializeComponent(F("WiFi"), []() -> ResourceResult {
          auto result = setupWiFi();
            if (!result.isSuccess()) {
              logger.error(F("main"), F("WiFi-Initialisierung fehlgeschlagen: ") +
                                          result.getMessage());
          }
          return result;
        })) {
      return;
    }
#endif

#if USE_DISPLAY
    // WiFi status is now shown in real-time during connection attempts
    if (displayManager) {
      if (isCaptivePortalAPActive()) {
        displayManager->updateLogStatus(F("AP-Modus aktiv"), false);
        displayManager->updateLogStatus(F("SSID: ") + WiFi.softAPSSID(), false);
        displayManager->updateLogStatus(F("IP: ") + WiFi.softAPIP().toString(),
                                        false);
      } else {
        displayManager->updateLogStatus(F("WiFi verbunden"), false);
        displayManager->updateLogStatus(F("SSID: ") + WiFi.SSID(), false);
        displayManager->updateLogStatus(F("IP: ") + WiFi.localIP().toString(),
                                        false);
      }
    }
#endif

    // Initialize minimal web server for OTA updates
    if (!Helper::initializeComponent(
            F("minimal web server"), []() -> ResourceResult {
              auto& webManager = WebManager::getInstance();
                if (!webManager.beginUpdateMode()) {
                  return ResourceResult::fail(
                      ResourceError::WEBSERVER_ERROR,
                      F("Initialisierung des minimalen Webservers fehlgeschlagen"));
              }
              return ResourceResult::success();
            })) {
      return;
    }

#if USE_DISPLAY
    // Show successful setup completion
    if (displayManager) {
      displayManager->updateLogStatus(F("Webserver bereit"), false);
      displayManager->updateLogStatus(F("Bereit für Updates"), false);
      delay(1000);  // Show completion message briefly
      displayManager->endUpdateMode();
    }
#endif

  logger.info(F("main"), F("Minimal-Update-Modus Setup abgeschlossen"));

#if USE_DISPLAY
    // Final status before exiting update mode
    if (displayManager) {
      displayManager->updateLogStatus(F("Update-Modus bereit"), false);
      delay(500);
      displayManager->endUpdateMode();
    }
#endif

    return;  // Exit setup() early - don't initialize other managers
  }

#if USE_DISPLAY
  // Inform user about normal mode on display
    if (displayManager) {
    displayManager->updateLogStatus(F("Normalmodus startet..."), true);
  }
#endif

#if USE_DISPLAY
  // Setup WiFi (normal mode) with display
  Helper::initializeComponent(F("WiFi"), []() -> ResourceResult {
    // Simplified approach without lambda callback
    auto result = setupWiFi();
      if (!result.isSuccess()) {
        logger.error(F("main"),
                     F("WiFi-Initialisierung fehlgeschlagen: ") + result.getMessage());
    }
    return result;
  });
#else
  // Setup WiFi (normal mode) without display
  Helper::initializeComponent(F("WiFi"), []() -> ResourceResult {
    auto result = setupWiFi();
    if (!result.isSuccess()) {
      logger.error(F("main"),
                   F("Failed to initialize WiFi: ") + result.getMessage());
    }
    return result;
  });
#endif
#if USE_DISPLAY
  if (displayManager) {
    if (isCaptivePortalAPActive()) {
      displayManager->updateLogStatus(F("AP-Modus aktiv"), true);
      displayManager->updateLogStatus(F("SSID: ") + WiFi.softAPSSID(), true);
      displayManager->updateLogStatus(F("IP: ") + WiFi.softAPIP().toString(),
                                      true);

      // WiFi connection attempts are now shown in real-time
      displayManager->updateLogStatus(F("WiFi einrichten:"), true);
      displayManager->updateLogStatus(F("1. Verbinde mit AP"), true);
      displayManager->updateLogStatus(
          F("2. Browser: ") + WiFi.softAPIP().toString(), true);
    } else {
      displayManager->updateLogStatus(F("WiFi verbunden"), true);
      displayManager->updateLogStatus(F("SSID: ") + WiFi.SSID(), true);
      displayManager->updateLogStatus(F("IP: ") + WiFi.localIP().toString(),
                                      true);
    }
  }
#endif

  // Initialize NTP time synchronization
#if USE_WIFI
  if (!isCaptivePortalAPActive()) {
#endif
    Helper::initializeComponent(F("NTP time sync"), []() -> ResourceResult {
      logger.info(F("main"), F("Initialisiere NTP-Zeitsynchronisation"));
      logger.initNTP();
      int timeSync = 0;
      while (timeSync < 10) {
        if (logger.getSynchronizedTime() >
            24 * 3600) {  // Time is after Jan 1, 1970
          // Verify timezone setup
          logger.verifyTimezone();
#if USE_DISPLAY
          if (displayManager)
            displayManager->updateLogStatus(F("NTP..."), true);
#endif
          return ResourceResult::success();
        }
        delay(1000);
        logger.updateNTP();
        timeSync++;
        logger.debug(F("main"), F("Warte auf Zeitsynchronisation..."));
      }
#if USE_DISPLAY
      if (displayManager)
        displayManager->updateLogStatus(F("NTP-Fehler"), true);
#endif
  logger.error(F("main"),
       F("NTP-Zeitsynchronisation fehlgeschlagen"));
  return ResourceResult::fail(ResourceError::TIME_SYNC_ERROR,
              F("Zeit konnte nicht synchronisiert werden"));
    });
#if USE_WIFI
  } else {
    logger.info(F("main"),
                F("Überspringe NTP-Initialisierung im AP/Captive-Portal-Modus"));
  }
#endif

  // Initialize sensor manager
  Helper::initializeComponent(F("sensor manager"), []() -> ResourceResult {
    sensorManager = std::make_unique<SensorManager>();
    auto result = sensorManager->init();
    if (!result.isSuccess()) {
      logger.error(F("main"), F("Sensor-Manager Initialisierung fehlgeschlagen: ") +
                                  result.getMessage());
#if USE_DISPLAY
      if (displayManager)
        displayManager->updateLogStatus(F("Sensor Fehler"), true);
#endif
    }
    return result;
  });
#if USE_DISPLAY
  if (displayManager) displayManager->updateLogStatus(F("Sensoren..."), true);
#endif

#if USE_DISPLAY
  if (displayManager) displayManager->logEnabledSensors();
#endif

#if USE_WEBSERVER
  // Initialize web manager
  Helper::initializeComponent(F("web manager"), []() -> ResourceResult {
    auto& webManager = WebManager::getInstance();
    if (sensorManager) {
      webManager.setSensorManager(*sensorManager);
      logger.debug(F("main"), F("Sensor-Manager im WebManager gesetzt"));
    } else {
      logger.error(F("main"),
                   F("Sensor-Manager ist null beim Setzen im WebManager"));
#if USE_DISPLAY
      if (displayManager)
        displayManager->updateLogStatus(F("Web Fehler"), true);
#endif
      return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                  F("Sensor manager not available"));
    }
    if (!webManager.begin()) {
#if USE_DISPLAY
      if (displayManager)
        displayManager->updateLogStatus(F("Web Fehler"), true);
#endif
    logger.error(
  F("main"),
  F("Web-Manager Initialisierung fehlgeschlagen: konnte nicht initialisiert werden"));
    return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                  F("Konnte nicht initialisieren"));
    }
    return ResourceResult::success();
  });
#if USE_DISPLAY
  if (displayManager) {
    displayManager->updateLogStatus(F("Webserver..."), true);

    // Add WiFi setup instructions if in AP mode
    if (isCaptivePortalAPActive()) {
      displayManager->updateLogStatus(F("WiFi Setup bereit"), true);
    }
  }
#endif
#endif

#if USE_MAIL
  if (!isCaptivePortalAPActive()) {
    Helper::initializeComponent(F("E-Mail"), []() -> ResourceResult {
      auto& mailManager = MailManager::getInstance();
      auto result = mailManager.init();
      if (!result.isSuccess()) {
#if USE_DISPLAY
        if (displayManager)
          displayManager->updateLogStatus(F("Mail Fehler"), true);
#endif
        logger.error(F("main"), F("E-Mail-Initialisierung fehlgeschlagen: ") +
                                    result.getMessage());
        return result;
      }
      return ResourceResult::success();
    });
#if USE_DISPLAY
    if (displayManager) displayManager->updateLogStatus(F("E-Mail..."), true);
#endif
  } else {
    logger.info(
      F("main"),
      F("Überspringe E-Mail-Initialisierung im AP/Captive-Portal-Modus"));
  }
#endif

#if USE_DISPLAY
  if (displayManager) displayManager->updateLogStatus(F("Ampel..."), true);
#endif

#if USE_DISPLAY
  if (displayManager) {
    if (isCaptivePortalAPActive()) {
      displayManager->updateLogStatus(F("Setup abgeschlossen"), true);
      displayManager->updateLogStatus(F("WiFi einrichten möglich"), true);
    } else {
      displayManager->updateLogStatus(F("Setup abgeschlossen"), true);
    }
  }
#endif

#if USE_DISPLAY
  if (displayManager) {
    // Add delay before ending boot mode so user can read the information
    delay(1000);  // 1 second delay
    displayManager->endBootMode();
  }
#endif

  logger.endMemoryTracking(F("managers_init"));
  logger.logMemoryStats(F("setup_complete"));
  logger.info(F("main"), F("Setup abgeschlossen"));

  // Sensor settings are now applied directly during JSON parsing
  // Trigger first measurement if sensor manager is ready and WiFi is connected
  if (sensorManager && sensorManager->getState() == ManagerState::INITIALIZED &&
      WiFi.status() == WL_CONNECTED) {
    logger.debug(F("main"), F("Starte initialen Messzyklus"));
    sensorManager->updateMeasurements();
  } else {
    logger.warning(F("main"), F("Überspringe initiale Messung – WiFi nicht verbunden oder Sensor-Manager nicht bereit"));
  }
}

void loop() {
  static unsigned long lastMemoryCheck = 0;
  static unsigned long lastMaintenanceCheck = 0;
  static unsigned long lastWiFiCheck = 0;
  static unsigned long lastMeasurementUpdate = 0;
  static unsigned long lastUpdateModeLog = 0;
  const unsigned long currentMillis = millis();

  // Handle update mode if active (now checked in setup(), but keep timeout
  // logic)
  if (ConfigMgr.getDoFirmwareUpgrade()) {
    // Debug: Log update mode recovery state (every 30 seconds)
    if (currentMillis - lastUpdateModeLog >= 30000) {
  logger.debug(F("main"),
       F("[UpdateMode] loop: getDoFirmwareUpgrade()=true"));
      auto& webManager = WebManager::getInstance();
      unsigned long updateStart = webManager.getUpdateModeStartTime();
      unsigned long timeout = webManager.getUpdateModeTimeout();
  logger.debug(F("main"), F("[UpdateMode] loop: currentMillis=") +
              String(currentMillis) + F(", updateStart=") +
              String(updateStart) + F(", timeout=") +
              String(timeout));
      if (updateStart > 0 && currentMillis - updateStart > timeout) {
  logger.warning(F("main"), F("Update-Mode Timeout erreicht. Beende Update-Modus automatisch."));
        ConfigMgr.setUpdateFlags(false, false);
        webManager.resetUpdateModeStartTime();
  logger.warning(F("main"), F("ESP startet neu."));
        ESP.restart();  // Force reboot to reload config and exit update mode
        return;
      } else {
    logger.debug(
      F("main"),
      F("[UpdateMode] loop: Kein Timeout, Update-Modus läuft weiter."));
      }
      lastUpdateModeLog = currentMillis;
    }
    WebManager::getInstance().handleClient();
    ESP.wdtFeed();
    delay(10);
    return;
  }

#if USE_WEBSOCKET
  // Handle WebSocket events first to ensure log messages are captured
  auto& ws = WebSocketService::getInstance();
  if (ws.isInitialized()) {
    ws.loop();
  }
#endif

  // Regular system checks and maintenance
  if (currentMillis - lastMemoryCheck >= 30000) {  // Every 30 seconds
    logger.logMemoryStats(F("loop_monitor"));
    lastMemoryCheck = currentMillis;

    // Emergency cleanup if memory is critically low
    if (ESP.getFreeHeap() < 3000) {
      logger.warning(F("main"),
                     F("Kritischer Speichermangel, führe Bereinigung durch"));
      if (sensorManager) {
        sensorManager->cleanup();
      }
    }
  }

  // WiFi connectivity check
  if (currentMillis - lastWiFiCheck >= 30000) {  // Every 30 seconds
#if USE_WIFI
    if (!isCaptivePortalAPActive()) {
      logger.debug(F("main"), F("Prüfe WiFi-Verbindung"));
      checkWiFiConnection();
    } else {
      logger.debug(F("main"),
                   F("AP-Modus aktiv, überspringe erneute WiFi-Verbindungsversuche"));
    }
#endif
    lastWiFiCheck = currentMillis;
  }

  // System maintenance tasks
  if (currentMillis - lastMaintenanceCheck >= 600000) {  // Every 10 minutes
  logger.debug(F("main"), F("Führe Wartungsaufgaben aus"));

#if USE_WIFI
    logger.updateNTP();
#endif
    lastMaintenanceCheck = currentMillis;
  }

// Handle web server requests
#if USE_WEBSERVER
  WebManager::getInstance().handleClient();
#endif

// Update display if enabled
#if USE_DISPLAY
  if (displayManager) {
    displayManager->update();
  }
#endif

  // Handle sensor measurements with a minimum delay between updates
  static constexpr unsigned long MEASUREMENT_UPDATE_INTERVAL =
      1000;  // 1s between measurement updates
  if (sensorManager && sensorManager->getState() == ManagerState::INITIALIZED &&
      WiFi.status() == WL_CONNECTED &&
      currentMillis - lastMeasurementUpdate >= MEASUREMENT_UPDATE_INTERVAL) {
    sensorManager->updateMeasurements();

    // Update LED traffic light status for mode 2
#if USE_LED_TRAFFIC_LIGHT
    if (ledTrafficLightManager) {
      ledTrafficLightManager->updateSelectedMeasurementStatus();
    }
#endif

    lastMeasurementUpdate = currentMillis;
  }

  // Basic system maintenance
  yield();
  delay(1);  // Prevent tight loop
}
