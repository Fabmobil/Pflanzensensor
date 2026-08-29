/**
 * @file main_web.cpp
 * @brief Web server initialization module
 * @details Handles web server setup, routing, authentication, and handler registration
 */

#include <Arduino.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_display.h"
#include "managers/manager_resource.h"
#include "managers/manager_sensor.h"
#include "web/core/web_manager.h"

// Forward declarations for globals defined in main.cpp
extern std::unique_ptr<SensorManager> sensorManager;
extern std::unique_ptr<DisplayManager> displayManager;
extern WebManager& webManager;

/**
 * @brief Initialize web server
 * @return ResourceResult indicating success or failure
 * @details
 * 1. Create WebManager instance
 * 2. Set sensor manager reference
 * 3. Initialize web server
 * 4. Register routes and handlers
 */
ResourceResult initializeWebServer() {
  logger.info(F("main_web"), F("Web-Manager Initialisierung gestartet"));

#if USE_WEBSERVER
  // Set sensor manager reference
  if (sensorManager && sensorManager->isHealthy()) {
    webManager.setSensorManager(*sensorManager);
    logger.debug(F("main_web"), F("Sensor-Manager im WebManager gesetzt"));
  } else {
    logger.error(F("main_web"),
                 F("Sensor-Manager ist null oder nicht gesund beim Setzen im WebManager"));
    return ResourceResult::fail(ResourceError::WEBSERVER_ERROR, F("Sensor manager not available"));
  }

  // Initialize web server
  ResourceResult result = webManager.begin();
  if (!result.isSuccess()) {
    logger.error(F("main_web"),
                 String(F("Web-Manager Initialisierung fehlgeschlagen: ")) + result.getMessage());
#if USE_DISPLAY
    if (displayManager) {
      displayManager->updateLogStatus(F("Web Fehler"), true);
    }
#endif
    return ResourceResult::fail(ResourceError::WEBSERVER_INIT_FAILED, result.getMessage());
  }

  logger.info(F("main_web"), F("Web-Manager Initialisierung erfolgreich"));
  return ResourceResult::success();
#else
  logger.info(F("main_web"), F("Webserver nicht aktiviert - Initialisierung übersprungen"));
  return ResourceResult::success();
#endif
}

/**
 * @brief Display web server status
 */
void showWebServerStatus() {
#if USE_WEBSERVER && USE_DISPLAY
  if (displayManager && webManager.isInitialized()) {
    displayManager->updateLogStatus(F("Webserver..."), true);
  }
#endif
}
