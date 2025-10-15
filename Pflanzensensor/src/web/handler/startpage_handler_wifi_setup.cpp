#include "../../logger/logger.h"
#include "../../managers/manager_config.h"
#include "../../utils/wifi.h"
#include "startpage_handler.h"

extern Logger logger;

void StartpageHandler::renderWiFiSetupForm() {
  // Add WiFi setup form when in AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    sendChunk(F("<div class='card wifi-setup-card'>"));
  sendChunk(F("<h3>📡 WiFi-Einrichtung</h3>"));
    sendChunk(F("<form method='POST' action='/admin/updateWiFi'>"));

    // Slot selection
    sendChunk(F("<label for='wifi_slot'>WiFi-Slot wählen:</label>"));
    sendChunk(
        F("<select name='wifi_slot' id='wifi_slot' required "
          "class='form-control'>"));

    // Get current WiFi credentials for display
    String ssid1 = ConfigMgr.getWiFiSSID1();
    String ssid2 = ConfigMgr.getWiFiSSID2();
    String ssid3 = ConfigMgr.getWiFiSSID3();

    // Get active slot
    int activeSlot = getActiveWiFiSlot();

    sendChunk(F("<option value='1'"));
    if (activeSlot == 1) sendChunk(F(" selected"));
    sendChunk(F(">Slot 1: "));
    sendChunk(ssid1.length() ? ssid1 : F("(leer)"));
    if (activeSlot == 1) sendChunk(F(" [AKTIV]"));
    sendChunk(F("</option>"));

    sendChunk(F("<option value='2'"));
    if (activeSlot == 2) sendChunk(F(" selected"));
    sendChunk(F(">Slot 2: "));
    sendChunk(ssid2.length() ? ssid2 : F("(leer)"));
    if (activeSlot == 2) sendChunk(F(" [AKTIV]"));
    sendChunk(F("</option>"));

    sendChunk(F("<option value='3'"));
    if (activeSlot == 3) sendChunk(F(" selected"));
    sendChunk(F(">Slot 3: "));
    sendChunk(ssid3.length() ? ssid3 : F("(leer)"));
    if (activeSlot == 3) sendChunk(F(" [AKTIV]"));
    sendChunk(F("</option>"));

    sendChunk(F("</select>"));
    sendChunk(F("</div>"));

    // WiFi network selection (scanned networks)
    sendChunk(F("<label for='wifi_ssid'>Verfügbare WiFi-Netzwerke:</label>"));
    sendChunk(
        F("<select name='wifi_ssid' id='wifi_ssid' required "
          "class='form-control'>"));
    sendChunk(F("<option value=''>Netzwerk auswählen...</option>"));

    // Scan for available WiFi networks
    int networkCount = WiFi.scanNetworks();
    if (networkCount > 0) {
      for (int i = 0; i < networkCount && i < 20;
           ++i) {  // Limit to 20 networks
        String networkSSID = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        uint8_t encType = WiFi.encryptionType(i);

        // Skip empty SSIDs
        if (networkSSID.length() == 0) continue;

        // Format signal strength
        String signalStrength;
        if (rssi >= -50)
          signalStrength = "Sehr gut";
        else if (rssi >= -60)
          signalStrength = "Gut";
        else if (rssi >= -70)
          signalStrength = "Mittel";
        else if (rssi >= -80)
          signalStrength = "Schwach";
        else
          signalStrength = "Sehr schwach";

        // Format security
        String security =
            (encType == ENC_TYPE_NONE) ? "Offen" : "Verschlüsselt";

        sendChunk(F("<option value='") + networkSSID + F("'>"));
        sendChunk(networkSSID + F(" (") + signalStrength + F(", ") + security +
                  F(")"));
        sendChunk(F("</option>"));
      }
    } else {
      sendChunk(F("<option value=''>Keine Netzwerke gefunden</option>"));
    }
    sendChunk(F("</select>"));
    sendChunk(F("</div>"));

    // Password input
    sendChunk(F("<label for='wifi_password'>Passwort:</label>"));
    sendChunk(
        F("<input type='password' name='wifi_password' id='wifi_password' "
          "required placeholder='WiFi-Passwort'>"));
    sendChunk(F("</div>"));

    // Submit button
    sendChunk(
        F("<button type='submit' class='button button-primary'>WiFi "
          "konfigurieren</button>"));
    sendChunk(F("</form>"));
    sendChunk(F("</div></div>"));
  }
}
