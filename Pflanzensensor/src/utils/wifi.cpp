/**
 * @file wifi.cpp
 * @brief Implementation of WiFi connection functions
 */

#include "utils/wifi.h"

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/mdns_name.h"

#include <ESP8266mDNS.h>

WiFiClient client;

bool apModeActive = false;
int g_activeWiFiSlot = -1; // -1 means not connected

// Track WiFi connection attempts for display
String g_wifiAttemptsInfo = "";

/**
 * @brief Attempt to connect to WiFi using up to 3 credentials.
 * @details Tries each SSID/PASSWORD pair in order. Returns true if connected,
 * false otherwise. Delegates to tryAllWiFiCredentialsWithDisplay with no callback.
 */
bool tryAllWiFiCredentials() { return tryAllWiFiCredentialsWithDisplay(nullptr); }

/**
 * @brief Start WiFi Access Point for manual configuration
 * @details Starts AP with HOSTNAME as SSID, no password, for manual WiFi setup.
 */
void startAPMode() {
  // Use the device name from configuration as the AP SSID so that the
  // SSID reflects what the user set in the Admin page. Fall back to the
  // compile-time HOSTNAME when the device name is empty.
  String deviceName = ConfigMgr.getDeviceName();
  // Clean up device name for SSID use
  deviceName.replace('\n', ' ');
  deviceName.replace('\r', ' ');
  deviceName.trim();
  if (deviceName.length() == 0) {
    deviceName = String(HOSTNAME);
  }
  // SSID must be <= 32 characters
  if (deviceName.length() > 32) {
    deviceName = deviceName.substring(0, 32);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(deviceName.c_str(), "", 1, 0, 1); // No password
  IPAddress apIP = WiFi.softAPIP();

  apModeActive = true;
  LOG_WARN(F("WiFi"), String(F("AP-Modus gestartet: ")) + deviceName);
  LOG_INFO(F("WiFi"), String(F("AP IP-Adresse: ")) + apIP.toString());
  LOG_INFO(F("WiFi"), String(F("WiFi-Setup erreichbar unter: ")) + apIP.toString());
}

/**
 * @brief Check if AP mode is active
 * @return true if AP mode is active
 */
bool isCaptivePortalAPActive() { return apModeActive; }

ResourceResult setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Vor dem Verbinden setzen: der Name geht mit der DHCP-Anfrage raus, danach
  // wäre es zu spät. Dadurch steht im Router "frameclaw-ps" statt "ESP-A4C138"
  // - unabhängig von mDNS, das erst später dazukommt.
  {
    char host[MdnsName::MAX_LEN + 1];
    MdnsName::hostnameVon(ConfigMgr.getDeviceName().c_str(), host, sizeof(host));
    WiFi.hostname(host);
  }

#if USE_STATIC_IP
  IPAddress ip(STATIC_IP);
  IPAddress gateway(GATEWAY);
  IPAddress subnet(SUBNET);
  IPAddress primaryDNS(PRIMARY_DNS);
  IPAddress secondaryDNS(SECONDARY_DNS);

  if (!WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS)) {
    LOG_ERROR(F("WiFi"), F("Statische IP-Konfiguration fehlgeschlagen"));
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("Static IP configuration failed"));
  }
#endif

  if (tryAllWiFiCredentials()) {
    apModeActive = false;
#ifdef ESP32
    // Configure DNS explicitly for ESP32-C6 (OpenThread DNS64 conflict workaround)
    // Use Google DNS and Cloudflare as fallback
    IPAddress dns1(8, 8, 8, 8); // Google DNS
    IPAddress dns2(1, 1, 1, 1); // Cloudflare DNS
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    LOG_DEBUG(F("WiFi"), F("DNS konfiguriert: 8.8.8.8, 1.1.1.1"));
#endif
    return ResourceResult::success();
  } else {
    // Wenn keine Verbindung hergestellt werden kann, starte den Access Point
    // damit Benutzer sich mit dem Gerät verbinden und die Admin-Seite nutzen
    // können. Das Captive-Portal wurde entfernt, nur der AP wird gestartet.
    startAPMode();
    return ResourceResult::fail(ResourceError::WIFI_ERROR,
                                F("Verbindungs-Timeout für alle Zugangsdaten; AP-Modus gestartet"));
  }
}

/**
 * @brief Nicht-blockierende WiFi-Verbindungsprüfung und Wiederherstellung
 * @details Verwendet eine Zustandsmaschine statt blockierender delay()-Schleifen.
 *          Wird alle 30s aus loop() aufgerufen. Bei Verbindungsverlust:
 *          1. WiFi.begin() mit letzten bekannten Zugangsdaten starten
 *          2. Beim nächsten Aufruf (30s später) prüfen ob verbunden
 *          3. Falls nicht verbunden: nächsten Credential-Slot versuchen
 *          4. Nach allen Slots: AP-Modus als Fallback
 *
 * WICHTIG: Blockiert NICHT den Web-Server. Jeder Aufruf kehrt sofort zurück.
 */
ResourceResult checkWiFiConnection() {
  // Zustandsvariablen für nicht-blockierende Wiederverbindung
  static int reconnectSlot = -1;           // Aktuell versuchter Credential-Slot (-1 = kein Versuch)
  static unsigned long reconnectStart = 0; // Zeitpunkt des WiFi.begin()-Aufrufs

  if (WiFi.status() == WL_CONNECTED) {
    // Verbindung steht — Zustand zurücksetzen
    if (reconnectSlot >= 0) {
      LOG_INFO(F("WiFi"), String(F("Verbindung wiederhergestellt: ")) + WiFi.SSID());
      LOG_INFO(F("WiFi"), String(F("IP-Adresse: ")) + WiFi.localIP().toString());
      reconnectSlot = -1;
    }
    return ResourceResult::success();
  }

  // --- Verbindung verloren ---

  unsigned long now = millis();

  // Erster Aufruf nach Verbindungsverlust: Letzten bekannten Slot versuchen
  if (reconnectSlot < 0) {
    reconnectSlot = (g_activeWiFiSlot >= 0) ? g_activeWiFiSlot : 0;
    LOG_WARN(F("WiFi"), String(F("Verbindung verloren. Starte Wiederherstellung ab Slot ")) +
                            String(reconnectSlot + 1));
  }

  // Prüfen ob letzter Versuch schon lange genug läuft (mind. 10s warten)
  if (reconnectStart > 0 && (now - reconnectStart) < 10000) {
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("Wiederverbindung läuft..."));
  }

  // Nächsten Slot versuchen
  bool foundCredentials = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    int slot = (reconnectSlot + attempt) % 3;
    String ssid = ConfigMgr.getWiFiSSID(slot + 1);
    String pwd = ConfigMgr.getWiFiPassword(slot + 1);

    if (!ssid.isEmpty() && !pwd.isEmpty()) {
      LOG_INFO(F("WiFi"), String(F("Versuche Slot ")) + String(slot + 1) + String(F(": ")) + ssid);
      WiFi.begin(ssid.c_str(), pwd.c_str());
      reconnectSlot = (slot + 1) % 3; // Nächster Slot beim nächsten Aufruf
      reconnectStart = now;
      foundCredentials = true;
      break;
    }
  }

  if (!foundCredentials) {
    LOG_ERROR(F("WiFi"), F("Keine WiFi-Zugangsdaten konfiguriert"));
    reconnectSlot = -1;
    reconnectStart = 0;
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("Keine Zugangsdaten konfiguriert"));
  }

  return ResourceResult::fail(ResourceError::WIFI_ERROR, F("Wiederverbindung gestartet"));
}

/**
 * @brief Nicht-blockierender Rückweg aus dem AP-Modus
 * @return ResourceResult::success() sobald wieder eine STA-Verbindung steht
 * @details Bisher war der AP-Modus eine Sackgasse: startAPMode() setzte
 *          apModeActive=true, loop() übersprang daraufhin dauerhaft
 *          checkWiFiConnection(), und nichts setzte das Flag je zurück. Ein
 *          Router-Neustart zum falschen Zeitpunkt strandete das Gerät bis zum
 *          nächsten Stromausfall im AP-Modus.
 *
 *          Diese Funktion probiert im Hintergrund weiter, das konfigurierte
 *          Netz zu erreichen. Sie blockiert nicht - der AP bleibt währenddessen
 *          nutzbar, damit die WiFi-Konfiguration über die Admin-Seite möglich
 *          bleibt. Erst wenn eine Verbindung tatsächlich steht, wird der AP
 *          abgeschaltet.
 */
ResourceResult checkAPModeRecovery() {
  static int retrySlot = 0;              // nächster zu probierender Slot (0-2)
  static unsigned long attemptStart = 0; // Zeitpunkt des laufenden WiFi.begin()
  static unsigned long lastAttempt = 0;  // Ende des letzten Versuchs

  if (!apModeActive) {
    return ResourceResult::success();
  }

  unsigned long now = millis();

  // Läuft gerade ein Versuch? Dann Ergebnis prüfen.
  if (attemptStart > 0) {
    if (WiFi.status() == WL_CONNECTED) {
      g_activeWiFiSlot = (retrySlot + 2) % 3; // der zuletzt gestartete Slot
      LOG_INFO(F("WiFi"), String(F("Netz wieder erreichbar, verlasse AP-Modus: ")) + WiFi.SSID());
      LOG_INFO(F("WiFi"), String(F("IP-Adresse: ")) + WiFi.localIP().toString());

      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apModeActive = false;
      attemptStart = 0;
      return ResourceResult::success();
    }

    // Versuch noch jung? Weiter warten.
    if (now - attemptStart < AP_RECOVERY_ATTEMPT_MS) {
      return ResourceResult::fail(ResourceError::WIFI_ERROR, F("AP-Rückweg: Versuch läuft"));
    }

    // Fehlgeschlagen - Pause bis zum nächsten Versuch
    attemptStart = 0;
    lastAttempt = now;
    WiFi.mode(WIFI_AP); // STA wieder abschalten, AP bleibt bestehen
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("AP-Rückweg: Versuch erfolglos"));
  }

  // Zwischen zwei Versuchen warten, damit der AP nicht dauernd gestört wird
  if (lastAttempt != 0 && now - lastAttempt < AP_RECOVERY_INTERVAL_MS) {
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("AP-Rückweg: Wartezeit"));
  }

  // Nächsten konfigurierten Slot suchen
  for (int i = 0; i < 3; i++) {
    int slot = (retrySlot + i) % 3;
    String ssid = ConfigMgr.getWiFiSSID(slot + 1);
    String pwd = ConfigMgr.getWiFiPassword(slot + 1);
    if (ssid.isEmpty() || pwd.isEmpty()) {
      continue;
    }

    LOG_INFO(F("WiFi"),
             String(F("AP-Modus aktiv - probiere Slot ")) + String(slot + 1) + F(" erneut"));

    // AP weiterlaufen lassen, damit die Konfiguration erreichbar bleibt
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pwd.c_str());
    retrySlot = (slot + 1) % 3;
    attemptStart = now;
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("AP-Rückweg: Versuch gestartet"));
  }

  return ResourceResult::fail(ResourceError::WIFI_ERROR, F("Keine Zugangsdaten konfiguriert"));
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

  WiFiServer testServer(port);
  testServer.begin();
  delay(100);
#ifdef ESP32
  // ESP32 WiFiServer doesn't have status() method
  bool isAvailable = true; // Assume available if begin() didn't throw
  testServer.end();
#else
  bool isAvailable = testServer.status() != CLOSED;
  testServer.close();
#endif
  return TypedResult<ResourceError, bool>::success(isAvailable);
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
    LOG_INFO(F("WiFi"), String(F("Verbinde mit WiFi: ")) + String(ssids[i]));

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
      LOG_DEBUG(F("WiFi"), F("."));
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
      LOG_INFO(F("WiFi"), String(F("Mit WiFi verbunden: ")) + String(ssids[i]));
      LOG_INFO(F("WiFi"), String(F("IP-Adresse: ")) + WiFi.localIP().toString());

      // Show success immediately
      if (displayCallback) {
        displayCallback("✓ Verbunden: " + String(ssids[i]), true);
      }

      // Update attempts info to show success
      g_wifiAttemptsInfo += " ✓";
      g_wifiAttemptsInfo += " → Verbindung erfolgreich";

      return true;
    } else {
      LOG_WARN(F("WiFi"), String(F("Verbindung mit WiFi fehlgeschlagen: ")) + String(ssids[i]));

      // Show failure immediately
      if (displayCallback) {
        displayCallback("✗ Timeout: " + String(ssids[i]), true);
      }

      // Update attempts info to show failure and reason
      g_wifiAttemptsInfo += " ✗ (Timeout)";
    }
  }

  g_activeWiFiSlot = -1;
  LOG_ERROR(F("WiFi"), F("Verbindung zu keinem konfigurierten WiFi-Netzwerk möglich"));

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

// === mDNS ===============================================================

namespace {

/// Was gerade angesagt wird. Leer heißt: der Responder läuft nicht.
String g_mdnsName;
/// IP, für die die Ansage gilt. Ändert sie sich, muss neu angesagt werden -
/// sonst schickt der Sensor Fragende weiterhin an seine alte Adresse.
IPAddress g_mdnsIp(0, 0, 0, 0);

} // namespace

String mdnsName() { return g_mdnsName; }

void aktualisiereMdns() {
  const bool verbunden = (WiFi.status() == WL_CONNECTED);
  const IPAddress ip = verbunden ? WiFi.localIP() : WiFi.softAPIP();

  // Ohne Adresse gibt es nichts anzusagen (z.B. kurz nach dem Start).
  if (ip == IPAddress(0, 0, 0, 0)) {
    if (g_mdnsName.length()) {
      MDNS.end();
      g_mdnsName = "";
      g_mdnsIp = IPAddress(0, 0, 0, 0);
    }
    return;
  }

  char host[MdnsName::MAX_LEN + 1];
  MdnsName::hostnameVon(ConfigMgr.getDeviceName().c_str(), host, sizeof(host));

  if (g_mdnsName != host || g_mdnsIp != ip) {
    if (g_mdnsName.length()) {
      MDNS.end();
    }
    if (MDNS.begin(host)) {
      // Der Dienstverweis ist nicht bloß Zierde: erst dadurch taucht der Sensor
      // in Netzwerkübersichten auf (Avahi, Bonjour, "Netzwerk" im Dateimanager).
      MDNS.addService(F("http"), F("tcp"), 80);
      g_mdnsName = host;
      g_mdnsIp = ip;
      LOG_INFO(F("mDNS"),
               String(F("Erreichbar als ")) + host + F(".local (") + ip.toString() + F(")"));
    } else {
      g_mdnsName = "";
      LOG_WARN(F("mDNS"), F("Responder liess sich nicht starten"));
    }
  }

  if (g_mdnsName.length()) {
    MDNS.update();
  }
}
