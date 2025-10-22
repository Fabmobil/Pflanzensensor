/**
 * @file wifi.cpp
 * @brief Implementation of WiFi connection functions
 */

#include "utils/wifi.h"

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"

WiFiClient client;

bool apModeActive = false;
int g_activeWiFiSlot = -1; // -1 means not connected

// Track WiFi connection attempts for display
String g_wifiAttemptsInfo = "";

/**
 * @brief Attempt to connect to WiFi using up to 3 credentials.
 * @details Tries each SSID/PASSWORD pair in order. Returns true if connected,
 * false otherwise.
 */
bool tryAllWiFiCredentials() {
  // Keep String objects alive for the duration of the function
  String ssid1 = ConfigMgr.getWiFiSSID1();
  String ssid2 = ConfigMgr.getWiFiSSID2();
  String ssid3 = ConfigMgr.getWiFiSSID3();
  String pwd1 = ConfigMgr.getWiFiPassword1();
  String pwd2 = ConfigMgr.getWiFiPassword2();
  String pwd3 = ConfigMgr.getWiFiPassword3();

  const char* ssids[] = {ssid1.c_str(), ssid2.c_str(), ssid3.c_str()};
  const char* passwords[] = {pwd1.c_str(), pwd2.c_str(), pwd3.c_str()};
  const int numCredentials = 3;

  // Reset connection attempts info
  g_wifiAttemptsInfo = "";

  // Check if any credentials are configured
  bool hasCredentials = false;
  for (int i = 0; i < numCredentials; ++i) {
    if (strlen(ssids[i]) > 0 && strlen(passwords[i]) > 0) {
      hasCredentials = true;
      break;
    }
  }

  if (!hasCredentials) {
    g_wifiAttemptsInfo = F("Keine WiFi-Zugangsdaten konfiguriert");
    return false;
  }

  for (int i = 0; i < numCredentials; ++i) {
    if (strlen(ssids[i]) == 0 || strlen(passwords[i]) == 0) {
      // Add empty slot info
      if (g_wifiAttemptsInfo.length() > 0) {
        g_wifiAttemptsInfo += ", ";
      }
      g_wifiAttemptsInfo += String(F("Slot ")) + String(i + 1) + F(": leer");
      continue; // Skip empty credentials
    }

    WiFi.begin(ssids[i], passwords[i]);
    logger.info(F("WiFi"), F("Verbinde mit WiFi: ") + String(ssids[i]));

    // Add to attempts info
    if (g_wifiAttemptsInfo.length() > 0) {
      g_wifiAttemptsInfo += F(", ");
    }
    g_wifiAttemptsInfo += String(F("Versuch ")) + String(i + 1) + F(": ") + String(ssids[i]);

    int attempts = 0;
    const int MAX_ATTEMPTS = 20; // 10 seconds (20 * 500ms)
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
      delay(500);
      logger.debug(F("WiFi"), F("."));
      attempts++;

      // Update display every 2 seconds (every 4 attempts)
      if (attempts % 4 == 0 && attempts > 0) {
        // This will be handled by the main loop calling updateDisplay
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      g_activeWiFiSlot = i;
      logger.info(F("WiFi"), F("Mit WiFi verbunden: %s") + String(ssids[i]));
      logger.info(F("WiFi"), F("IP-Adresse: %s") + WiFi.localIP().toString());

      // Update attempts info to show success
      g_wifiAttemptsInfo += F(" ✓");

      // Add final result for successful connection
      g_wifiAttemptsInfo += F(" → Verbindung erfolgreich");

      return true;
    } else {
      logger.warning(F("WiFi"), F("Verbindung mit WiFi fehlgeschlagen: ") + String(ssids[i]));

      // Update attempts info to show failure and reason
      g_wifiAttemptsInfo += F(" ✗ (Timeout)");
    }
  }

  g_activeWiFiSlot = -1;
  logger.error(F("WiFi"), F("Verbindung zu keinem konfigurierten WiFi-Netzwerk möglich"));

  // Add final result to attempts info
  if (g_wifiAttemptsInfo.length() > 0) {
    g_wifiAttemptsInfo += F(" → Alle Versuche fehlgeschlagen");
  }

  return false;
}

/**
 * @brief Start WiFi Access Point for manual configuration
 * @details Starts AP with HOSTNAME as SSID, no password, for manual WiFi setup.
 */
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(HOSTNAME); // No password
  IPAddress apIP = WiFi.softAPIP();

  apModeActive = true;
  logger.warning(F("WiFi"), F("AP-Modus gestartet: ") + String(HOSTNAME));
  logger.info(F("WiFi"), F("AP IP-Adresse: ") + apIP.toString());
  logger.info(F("WiFi"), F("WiFi-Setup erreichbar unter: ") + apIP.toString());
}

/**
 * @brief Check if AP mode is active
 * @return true if AP mode is active
 */
bool isCaptivePortalAPActive() { return apModeActive; }

ResourceResult setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

#if USE_STATIC_IP
  IPAddress ip(STATIC_IP);
  IPAddress gateway(GATEWAY);
  IPAddress subnet(SUBNET);
  IPAddress primaryDNS(PRIMARY_DNS);
  IPAddress secondaryDNS(SECONDARY_DNS);

  if (!WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS)) {
    logger.error(F("WiFi"), F("Statische IP-Konfiguration fehlgeschlagen"));
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("Static IP configuration failed"));
  }
#endif

  if (tryAllWiFiCredentials()) {
    apModeActive = false;
    return ResourceResult::success();
  } else {
    startAPMode();
    return ResourceResult::fail(ResourceError::WIFI_ERROR,
                                F("Verbindungs-Timeout für alle Zugangsdaten; AP-Modus gestartet"));
  }
}

ResourceResult checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    logger.warning(F("WiFi"), F("WiFi-Verbindung verloren. Stelle erneut her..."));
    WiFi.disconnect();
    if (tryAllWiFiCredentials()) {
      return ResourceResult::success();
    } else {
      return ResourceResult::fail(ResourceError::WIFI_ERROR,
                                  F("Erneutes Verbindungs-Timeout für alle Zugangsdaten"));
    }
  }
  return ResourceResult::success();
}

TypedResult<ResourceError, int> getWiFiSignalStrength() {
  if (WiFi.status() != WL_CONNECTED) {
    return TypedResult<ResourceError, int>::fail(ResourceError::WIFI_ERROR,
                                                 F("WiFi nicht verbunden"));
  }
  return TypedResult<ResourceError, int>::success(WiFi.RSSI());
}

TypedResult<ResourceError, bool> checkPort(uint16_t port) {
  if (WiFi.status() != WL_CONNECTED) {
    return TypedResult<ResourceError, bool>::fail(ResourceError::WIFI_ERROR,
                                                  F("WiFi nicht verbunden"));
  }

  try {
    WiFiServer testServer(port);
    testServer.begin();
    delay(100);
    bool isAvailable = testServer.status() != CLOSED;
    testServer.close();
    return TypedResult<ResourceError, bool>::success(isAvailable);
  } catch (...) {
    return TypedResult<ResourceError, bool>::fail(ResourceError::OPERATION_FAILED,
                                                  F("Port check failed"));
  }
}

int getActiveWiFiSlot() { return g_activeWiFiSlot; }

String getWiFiConnectionAttemptsInfo() { return g_wifiAttemptsInfo; }

String getCurrentWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    return "WiFi verbunden: " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")";
  } else if (apModeActive) {
    return "AP-Modus: " + WiFi.softAPSSID() + " (" + WiFi.softAPIP().toString() + ")";
  } else {
    return "WiFi nicht verbunden";
  }
}

bool tryAllWiFiCredentialsWithDisplay(std::function<void(const String&, bool)> displayCallback) {
  String pwd1 = ConfigMgr.getWiFiPassword1();
  String pwd2 = ConfigMgr.getWiFiPassword2();
  String pwd3 = ConfigMgr.getWiFiPassword3();
  String ssid1 = ConfigMgr.getWiFiSSID1();
  String ssid2 = ConfigMgr.getWiFiSSID2();
  String ssid3 = ConfigMgr.getWiFiSSID3();

  const char* ssids[] = {ssid1.c_str(), ssid2.c_str(), ssid3.c_str()};
  const char* passwords[] = {pwd1.c_str(), pwd2.c_str(), pwd3.c_str()};
  const int numCredentials = 3;

  // Reset connection attempts info
  g_wifiAttemptsInfo = "";

  // Check if any credentials are configured
  bool hasCredentials = false;
  for (int i = 0; i < numCredentials; ++i) {
    if (strlen(ssids[i]) > 0 && strlen(passwords[i]) > 0) {
      hasCredentials = true;
      break;
    }
  }

  if (!hasCredentials) {
    g_wifiAttemptsInfo = "Keine WiFi-Credentials konfiguriert";
    if (displayCallback) {
      displayCallback("Keine WiFi-Credentials konfiguriert", true);
    }
    return false;
  }

  for (int i = 0; i < numCredentials; ++i) {
    if (strlen(ssids[i]) == 0 || strlen(passwords[i]) == 0) {
      // Add empty slot info
      if (g_wifiAttemptsInfo.length() > 0) {
        g_wifiAttemptsInfo += ", ";
      }
      g_wifiAttemptsInfo += "Slot " + String(i + 1) + ": leer";

      // Show empty slot immediately
      if (displayCallback) {
        displayCallback("Slot " + String(i + 1) + ": leer", true);
      }
      continue; // Skip empty credentials
    }

    WiFi.begin(ssids[i], passwords[i]);
    logger.info(F("WiFi"), F("Verbinde mit WiFi: %s") + String(ssids[i]));

    // Show connection attempt immediately
    if (displayCallback) {
      displayCallback("Versuch " + String(i + 1) + ": " + String(ssids[i]), true);
    }

    // Add to attempts info
    if (g_wifiAttemptsInfo.length() > 0) {
      g_wifiAttemptsInfo += ", ";
    }
    g_wifiAttemptsInfo += "Versuch " + String(i + 1) + ": " + String(ssids[i]);

    int attempts = 0;
    const int MAX_ATTEMPTS = 20; // 10 seconds (20 * 500ms)
    while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
      delay(500);
      logger.debug(F("WiFi"), F("."));
      attempts++;

      // Show progress every 2 seconds
      if (attempts % 4 == 0 && attempts > 0) {
        if (displayCallback) {
          displayCallback("...", true);
        }
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      g_activeWiFiSlot = i;
      logger.info(F("WiFi"), F("Mit WiFi verbunden: %s") + String(ssids[i]));
      logger.info(F("WiFi"), F("IP-Adresse: %s") + WiFi.localIP().toString());

      // Show success immediately
      if (displayCallback) {
        displayCallback("✓ Verbunden: " + String(ssids[i]), true);
      }

      // Update attempts info to show success
      g_wifiAttemptsInfo += " ✓";
      g_wifiAttemptsInfo += " → Verbindung erfolgreich";

      return true;
    } else {
      logger.warning(F("WiFi"), F("Verbindung mit WiFi fehlgeschlagen: %s") + String(ssids[i]));

      // Show failure immediately
      if (displayCallback) {
        displayCallback("✗ Timeout: " + String(ssids[i]), true);
      }

      // Update attempts info to show failure and reason
      g_wifiAttemptsInfo += " ✗ (Timeout)";
    }
  }

  g_activeWiFiSlot = -1;
  logger.error(F("WiFi"), F("Verbindung zu keinem konfigurierten WiFi-Netzwerk möglich"));

  // Show final failure
  if (displayCallback) {
    displayCallback("Alle Versuche fehlgeschlagen", true);
  }

  // Add final result to attempts info
  if (g_wifiAttemptsInfo.length() > 0) {
    g_wifiAttemptsInfo += " → Alle Versuche fehlgeschlagen";
  }

  return false;
}
