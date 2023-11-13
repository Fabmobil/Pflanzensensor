/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul
 */

#include <ESP8266WiFi.h> // für WLAN
#include <ESP8266WebServer.h> // für Webserver

WiFiClient client;
ESP8266WebServer Webserver(80); //
/*
 * Funktion: String WebseiteKopfAusgeben()
 * Liest die /HTML/header.html Datei aus, formatiert sie um
 * und gibt sie als String zurück.
 */
String WebseiteKopfAusgeben() {
  #include "modul_wifi_header.h"
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von WebsiteKopfAusgeben()"));
    Serial.println(F("#######################################"));
    Serial.println(F("HTML-Code:"));
    Serial.println(htmlHeader);
    Serial.println(F("#######################################"));
  #endif
  return htmlHeader;
}

/*
 * Funktion: Void WebseiteStartAusgeben()
 * Gibt die Startseite des Webservers aus.
 */
void WebseiteStartAusgeben() {
  #include "modul_wifi_header.h"
  #include "modul_wifi_footer.h"
  String formatierterCode = htmlHeader;
  #if MODUL_LICHTSENSOR
    int lichtstaerke = 20;
    formatierterCode += "<h2>Lichtstärke</h2><p>";
    formatierterCode += lichtstaerke;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_BODENFEUCHTE
    int bodenfeuchte = 35;
    formatierterCode += "<h2>Bodenfeuchte</h2><p>";
    formatierterCode += bodenfeuchte;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_DHT
    int luftfeuchte = 37;
    int lufttemperatur = 22;
    formatierterCode += "<h2>Lufttemperatur</h2><p>";
    formatierterCode += lufttemperatur;
    formatierterCode += "°C</p>";
    formatierterCode += "<h2>Luftfeuchte</h2><p>";
    formatierterCode += luftfeuchte;
    formatierterCode += "%</p>";
  #endif
  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
}

/*
 * Funktion: Void WebseiteAdminAusgeben()
 * Gibt die Administrationsseite des Webservers aus.
 */
void WebseiteAdminAusgeben() {
  #include "modul_wifi_header.h"
  #include "modul_wifi_footer.h"
  String formatierterCode = htmlHeader;
  formatierterCode += "<h2>Adminseite</h2>";
  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
}

/*
 * Funktion: WifiSetup()
 * Verbindet das WLAN
 */
void WifiSetup(){
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von WifiSetup()"));
    Serial.println(F("#######################################"));
  #endif
// WLAN Verbindung herstellen
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_AP_STA);
  if (WiFi.status() == WL_CONNECTED) Serial.println("WLAN war verbunden");
  WiFi.begin(wifiSsid, wifiPassword);
  int i=0; // Es wird nur 20 mal versucht, eine WLAN Verbindung aufzubauen
  while (!(WiFi.status() == WL_CONNECTED) && i<20) {
      #if MODUL_DEBUG
        Serial.print("Verbindungsversuch ");
        Serial.print(i);
        Serial.println(" von 20.");
        delay(1000);
      #endif
      i++;
  }
  if (WiFi.status() != WL_CONNECTED) Serial.println("Keine WLAN-Verbindung möglich.");
  // Nun sollte WLAN verbunden sein
  Serial.print("meine IP: ");
  Serial.println(WiFi.localIP());
  Webserver.on("/", WebseiteStartAusgeben);
  Webserver.on("/admin.html", WebseiteAdminAusgeben);
  Webserver.begin(); // Webserver starten
  #if MODUL_DEBUG
    Serial.println(F("#######################################"));
  #endif
}


/*
 * Funktion: ifttt_nachricht(int bodenfeuchte, int lichtstaerke, int luftfeuchte, int lufttemperatur)
 * Sendet Nachrichten über einen www.ifttt.com Webhook
 * bodenfeuchte: Bodenfeuchte in %
 * lichtstaerke: Lichtstaerke in %
 * luftfeuchte: Luftfeuchte in %
 * lufttemperatur: Lufttemperatur in °C
 */
void ifttt_nachricht(int bodenfeuchte, int lichtstaerke, int luftfeuchte, int lufttemperatur) {
  // JSON Datei zusammenbauen:
  String jsonString = "";
  jsonString += "{\"bodenfeuchte:\":\"";
  jsonString += bodenfeuchte;
  jsonString += "\",\"lichtstaerke:\":\"";
  jsonString += lichtstaerke;
  jsonString += "\",\"luftfeuchte\":\"";
  jsonString += luftfeuchte;
  jsonString += "\",\"lufttemperatur\":\"";
  jsonString += lufttemperatur;
  jsonString += "\"}";
  int jsonLength = jsonString.length();
  String lenString = String(jsonLength);
  // connect to the Maker event server
  client.connect("maker.ifttt.com", 80);
  // construct the POST request
  String postString = "";
  postString += "POST /trigger/";
  postString += wifiIftttEreignis;
  postString += "/with/key/";
  postString += wifiIftttEreignis;
  postString += " HTTP/1.1\r\n";
  postString += "Host: maker.ifttt.com\r\n";
  postString += "Content-Type: application/json\r\n";
  postString += "Content-Length: ";
  postString += lenString + "\r\n";
  postString += "\r\n";
  postString += jsonString; // combine post request and JSON

  client.print(postString);
  delay(500);
  client.stop();
}
