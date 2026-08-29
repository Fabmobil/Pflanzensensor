/**
 * @file main.cpp
 * @brief Main program for ESP8266 based sensor system
 * @details Modularized initialization with separate files for:
 *          - System initialization (main_init.cpp)
 *          - WiFi/NTP (main_wifi.cpp)
 *          - Sensors (main_sensors.cpp)
 *          - Web server (main_web.cpp)
 *          - LED traffic light (main_led.cpp)
 */

// Arduino & ESP Core
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <LittleFS.h>
#include <Wire.h>

// System Components
#include "configs/config.h"
#include "utils/helper.h"
#include "utils/result_types.h"

// Manager Classes
#include "managers/manager_config.h"
#include "managers/manager_display.h"
#include "managers/manager_led_traffic_light.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"

// Network & Services
#if USE_WIFI
#include "utils/wifi.h"
#endif

#if USE_WEBSERVER
#include "web/core/web_manager.h"
#endif

// Prometheus Metrics
#if USE_PROMETHEUS_METRICS
#include "metrics/prometheus_metrics.h"
#endif

// Memory Management
#include "utils/memory_manager.h"

// Modular initialization files
#include "main_init.h"
#include "main_led.h"
#include "main_sensors.h"
#include "main_web.h"
#include "main_wifi.h"

// Global objects
extern std::unique_ptr<SensorManager> sensorManager;
extern ResourceManager& ResourceMgr;
extern MemoryManager& MemoryMgr;
#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;
#endif
#if USE_LED_TRAFFIC_LIGHT
extern std::unique_ptr<LedTrafficLightManager> ledTrafficLightManager;
#endif

/**
 * @brief System setup - Initialize all components
 */
void setup() {
  // Initialize serial communication first
  Serial.begin(115200);
  Serial.setDebugOutput(true); // Enable debug output to serial
  delay(1000);                 // Give sensors time to power up and stabilize

  // Initialize memory manager FIRST
  MemoryMgr.init();
  MemoryMgr.logState("setup_start");

  // **ULTRA-CRITICAL: Check for flash restore BEFORE ANYTHING ELSE**
  if (!initializeSystem()) {
    return; // Failed or rebooted
  }

  // Filesystem initialized
  logger.info(F("main"), F("Initialisiere Dateisystem"));

  // Initialize display (optional, don't fail on error)
#if USE_DISPLAY
  if (!Helper::initializeComponent(F("display manager"), []() -> ResourceResult {
        displayManager = std::make_unique<DisplayManager>();
        return displayManager->init();
      })) {
    // Note: Don't return here - display is optional
    logger.warning(F("main"), F("Display-Manager Initialisierung fehlgeschlagen, fahre fort"));
  } else {
    displayManager->showLogScreen(F("Filesystem..."), true);
  }
#endif

  // Initialize LED traffic light (optional, don't fail on error)
#if USE_LED_TRAFFIC_LIGHT
  if (!Helper::initializeComponent(F("LED traffic light manager"), []() -> ResourceResult {
        ledTrafficLightManager = std::make_unique<LedTrafficLightManager>();
        auto result = ledTrafficLightManager->init();
        if (!result.isSuccess()) {
          logger.warning(F("main"),
                         String(F("LED-Ampel-Manager Initialisierung fehlgeschlagen: ")) +
                             result.getMessage());
        }
        return result;
      })) {
    return;
  }
#endif

  // Show boot progress
  showBootProgress(F("Config..."));

  // Initialize configuration
  if (!Helper::initializeComponent(F("configuration"), []() -> ResourceResult {
        auto result = ConfigMgr.loadConfig();
        if (!result.isSuccess()) {
          logger.error(F("main"), String(F("Konfiguration konnte nicht geladen werden: ")) +
                                      result.getMessage());
          return ResourceResult::fail(ResourceError::CONFIG_ERROR, result.getMessage());
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
  if (displayManager)
    displayManager->updateLogStatus(F("Config..."), true);
#endif

  // Increase reboot count
  Helper::incrementRebootCount();

  // Handle boot loop recovery: clear firmware upgrade flag if stuck
  if (LittleFS.exists("/.clear_upgrade_flag")) {
    LittleFS.remove("/.clear_upgrade_flag");
    if (ConfigMgr.getDoFirmwareUpgrade()) {
      logger.warning(F("main"), F("Boot-Schleife erkannt: Firmware-Upgrade-Flag wird gelöscht"));
      ConfigMgr.setDoFirmwareUpgrade(false);
    }
  }

  // **CRITICAL FIX: Check for update mode BEFORE initializing heavy managers**
  if (ConfigMgr.getDoFirmwareUpgrade()) {
    logger.info(F("main"), F("Firmware-Upgrade-Modus erkannt - wechsle in Minimalmodus"));

#if USE_DISPLAY
    // Inform user about update mode on display
    if (displayManager) {
      displayManager->showLogScreen(F("Firmware-Update-Modus"), false);
      displayManager->updateLogStatus(F("Starte Minimal-Setup..."), false);
    }
#endif

    // Setup WiFi for update mode
    auto wifiResult = setupWiFiWithDisplay(displayManager != nullptr);
    if (!wifiResult.isSuccess()) {
      logger.error(F("main"), F("WiFi-Initialisierung für Update-Modus fehlgeschlagen"));
    }

#if USE_DISPLAY
    // WiFi status display
    if (displayManager) {
      if (WiFi.status() == WL_CONNECTED) {
        displayManager->updateLogStatus(F("WiFi verbunden"), false);
        displayManager->updateLogStatus(String(F("SSID: ")) + WiFi.SSID(), false);
        displayManager->updateLogStatus(String(F("IP: ")) + WiFi.localIP().toString(), false);
      } else {
        displayManager->updateLogStatus(F("WiFi nicht verbunden"), false);
      }
    }
#endif

    // Initialize minimal web server for OTA updates
    if (!Helper::initializeComponent(F("minimal web server"), []() -> ResourceResult {
          auto& webManager = WebManager::getInstance();
          if (!webManager.beginUpdateMode()) {
            return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                        F("Initialisierung des minimalen "
                                          "Webservers fehlgeschlagen"));
          }
          return ResourceResult::success();
        })) {
      return;
    }

#if USE_DISPLAY
    // Show successful setup completion
    if (displayManager) {
      displayManager->updateLogStatus(F("Webserver bereit"), false);
      delay(500);
      displayManager->endUpdateMode();
    }
#endif

    logger.info(F("main"), F("Minimal-Update-Modus Setup abgeschlossen"));
    return; // Exit setup() early - don't initialize other managers
  }

#if USE_DISPLAY
  // Inform user about normal mode on display
  if (displayManager) {
    displayManager->updateLogStatus(F("Normalmodus startet..."), true);
  }
#endif

  // Initialize WiFi first (required for web server and NTP)
  showBootProgress(F("WiFi..."));
  auto wifiResult = setupWiFiWithDisplay(displayManager != nullptr);
  if (!wifiResult.isSuccess()) {
    logger.warning(F("main"),
                   String(F("WiFi-Initialisierung fehlgeschlagen: ")) + wifiResult.getMessage());
    // Continue anyway - AP mode may be active for configuration
  }

  // Initialize all major subsystems
  initializeLedTrafficLight();
  initializeSensors();
  initializeWebServer();

#if USE_PROMETHEUS_METRICS
  // Initialize Prometheus metrics system
  PrometheusMetrics::getInstance().begin();
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
    delay(1000); // 1 second delay
    displayManager->endBootMode();
  }
#endif

  logger.endMemoryTracking(F("managers_init"));
  logger.logMemoryStats(F("setup_complete"));
  logger.info(F("main"), F("Setup abgeschlossen"));

  // Sensor settings are now applied directly during JSON parsing
  // DO NOT trigger a synchronous initial measurement here - it may block
  // the webserver while the measurement cycle (and associated LittleFS
  // flushes) complete. Measurements will be handled in loop() periodically
  // and start immediately there, keeping the webserver responsive.
  if (sensorManager && sensorManager->getState() == ManagerState::INITIALIZED) {
  }
}

/**
 * @brief Main loop - Handle all recurring tasks
 */
void loop() {
  static unsigned long lastMemoryCheck = 0;
  static unsigned long lastWiFiCheck = 0;
  static unsigned long lastMeasurementUpdate = 0;
  static unsigned long lastUpdateModeLog = 0;
  static unsigned long lastMemoryLog = 0;
  static bool bootLoopCounterCleared = false;
  const unsigned long currentMillis = millis();

  // Clear boot loop counter after 60s of stable uptime
  if (!bootLoopCounterCleared && currentMillis > 60000) {
    LittleFS.remove("/.boot_loop");
    bootLoopCounterCleared = true;
  }

  // Memory monitoring - check every 30 seconds
  if (currentMillis - lastMemoryCheck >= 30000) {
    // Notfall-Bereinigung bei niedrigem Heap
    MemoryMgr.checkAndCleanup(4000);

    auto metrics = MemoryMgr.getMetrics();

    // Log detailed memory state every 2 minutes
    if (currentMillis - lastMemoryLog >= 120000) {
      MemoryMgr.logState("loop");
      lastMemoryLog = currentMillis;
    }

    // Check for issues
    if (MemoryMgr.isCritical()) {
      logger.error(F("main"), F("CRITICAL: Heap below 3KB! "));
      if (sensorManager) {
        sensorManager->cleanup();
      }
    }

    if (MemoryMgr.isHighFragmentation()) {
      logger.warning(F("main"), F("High fragmentation: "));
      MemoryMgr.checkMemory();
    }

    lastMemoryCheck = currentMillis;
  }

  // Handle update mode if active
  if (ConfigMgr.getDoFirmwareUpgrade()) {
    // Debug: Log update mode recovery state (every 30 seconds)
    if (currentMillis - lastUpdateModeLog >= 30000) {
      logger.debug(F("main"), F("[UpdateMode] loop: getDoFirmwareUpgrade()=true"));
      auto& webManager = WebManager::getInstance();
      unsigned long updateStart = webManager.getUpdateModeStartTime();
      unsigned long timeout = webManager.getUpdateModeTimeout();
      logger.debug(F("main"), String(F("[UpdateMode] loop: currentMillis=")) +
                                  String(currentMillis) + String(F(", updateStart=")) +
                                  String(updateStart) + String(F(", timeout=")) + String(timeout));
      if (updateStart > 0 && currentMillis - updateStart > timeout) {
        logger.warning(F("main"), F("Update-Mode Timeout erreicht. Beende "
                                    "Update-Modus automatisch."));
        ConfigMgr.setUpdateFlags(false, false);
        webManager.resetUpdateModeStartTime();
        logger.warning(F("main"), F("ESP startet neu."));
        ESP.restart(); // Force reboot to reload config and exit update mode
        return;
      }
      lastUpdateModeLog = currentMillis;
    }
    WebManager::getInstance().handleClient();
    yield(); // Allow background tasks without blocking upload
    return;
  }

#if USE_WEBSOCKET
  // Handle WebSocket events first to ensure log messages are captured
  auto& ws = WebSocketService::getInstance();
  if (ws.isInitialized()) {
    ws.loop();
  }
#endif

  // WiFi connectivity check
  if (currentMillis - lastWiFiCheck >= 30000) { // Every 30 seconds
#if USE_WIFI
    if (!isCaptivePortalAPActive()) {
      logger.debug(F("main"), F("Prüfe WiFi-Verbindung"));
      checkWiFiConnection();
    } else {
      logger.debug(F("main"), F("AP-Modus aktiv, überspringe erneute WiFi-Verbindungsversuche"));
      yield();
    }
#endif
    lastWiFiCheck = currentMillis;
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

  // Handle sensor measurements
  static constexpr unsigned long MEASUREMENT_UPDATE_INTERVAL =
      1000; // 1s between measurement updates
  if (sensorManager && sensorManager->getState() == ManagerState::INITIALIZED &&
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
  delay(1); // Prevent tight loop
}
