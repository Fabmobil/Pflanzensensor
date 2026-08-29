/**
 * @file main_wifi.cpp
 * @brief WiFi and NTP initialization module
 * @details Handles WiFi connection, captive portal setup, and time synchronization
 */

#include <Arduino.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_display.h"
#include "managers/manager_resource.h"
#include "utils/wifi.h"

#if USE_DISPLAY
// Forward declaration – nur wenn Display aktiv (incomplete type sonst)
extern std::unique_ptr<DisplayManager> displayManager;
#endif

/**
 * @brief Setup WiFi connection with display support
 * @param showDisplay Whether to show status on display
 * @return ResourceResult indicating success or failure
 * @note This wraps the original ::setupWiFi() to add display feedback
 */
ResourceResult setupWiFiWithDisplay(bool showDisplay = false) {
  logger.info(F("main_wifi"), F("WiFi Initialisierung gestartet"));

#if USE_WIFI
  // Call original setupWiFi from utils/wifi.h (explicitly avoid this wrapper)
  auto result = ::setupWiFi();
  if (!result.isSuccess()) {
    logger.error(F("main_wifi"),
                 String(F("WiFi-Initialisierung fehlgeschlagen: ")) + result.getMessage());
    return ResourceResult::fail(ResourceError::WIFI_ERROR, result.getMessage());
  }

#if USE_DISPLAY
  if (showDisplay && displayManager) {
    if (isCaptivePortalAPActive()) {
      displayManager->updateLogStatus(F("AP-Modus aktiv"), true);
      // Show the AP SSID so users can identify the network to join
      displayManager->updateLogStatus(String(F("SSID: ")) + WiFi.softAPSSID(), true);
      displayManager->updateLogStatus(String(F("IP: ")) + WiFi.softAPIP().toString(), true);

      // WiFi connection attempts are now shown in real-time
      displayManager->updateLogStatus(F("WiFi einrichten:"), true);
      displayManager->updateLogStatus(F("1. Verbinde mit AP"), true);
      displayManager->updateLogStatus(String(F("2. Browser: ")) + WiFi.softAPIP().toString(), true);
    } else {
      displayManager->updateLogStatus(F("WiFi verbunden"), true);
      displayManager->updateLogStatus(String(F("SSID: ")) + WiFi.SSID(), true);
      displayManager->updateLogStatus(String(F("IP: ")) + WiFi.localIP().toString(), true);
    }
  }
#endif

  // Initialize NTP time synchronization
  if (WiFi.status() == WL_CONNECTED && !isCaptivePortalAPActive()) {
    logger.info(F("main_wifi"), F("NTP-Zeitsynchronisation gestartet"));

    logger.initNTP();
    int timeSync = 0;
    while (timeSync < 10) {
      if (logger.getSynchronizedTime() > 24 * 3600) { // Time is after Jan 1, 1970
        // Verify timezone setup
        logger.verifyTimezone();
#if USE_DISPLAY
        if (showDisplay && displayManager) {
          displayManager->updateLogStatus(F("NTP..."), true);
        }
#endif
        logger.info(F("main_wifi"), F("NTP-Zeitsynchronisation erfolgreich"));
        return ResourceResult::success();
      }
      delay(1000);
      logger.updateNTP();
      timeSync++;
      logger.debug(F("main_wifi"), F("Warte auf Zeitsynchronisation..."));
    }

    logger.error(F("main_wifi"), F("NTP-Zeitsynchronisation fehlgeschlagen"));
#if USE_DISPLAY
    if (showDisplay && displayManager) {
      displayManager->updateLogStatus(F("NTP-Fehler"), true);
    }
#endif
    return ResourceResult::fail(ResourceError::TIME_SYNC_ERROR,
                                F("Zeit konnte nicht synchronisiert werden"));
  } else {
    logger.info(F("main_wifi"),
                F("WiFi nicht verbunden oder AP-Modus - NTP-Initialisierung übersprungen"));
    return ResourceResult::success();
  }
#else
  logger.warning(F("main_wifi"), F("WiFi nicht aktiviert - Initialisierung übersprungen"));
  return ResourceResult::success();
#endif
}
