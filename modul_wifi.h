/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul
 */

#include <ESP8266WiFi.h> // für WLAN
#include <ESP8266WebServer.h> // für Webserver
#include <ESP8266mDNS.h> // für Namensauflösung

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
    formatierterCode += "<h2>Helligkeit</h2><p>";
    formatierterCode += messwertHelligkeit;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_BODENFEUCHTE
    formatierterCode += "<h2>Bodenfeuchte</h2><p>";
    formatierterCode += messwertBodenfeuchte;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_DHT
    formatierterCode += "<h2>Lufttemperatur</h2><p>";
    formatierterCode += messwertLufttemperatur;
    formatierterCode += "°C</p>";
    formatierterCode += "<h2>Luftfeuchte</h2><p>";
    formatierterCode += messwertLuftfeuchte;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_LEDAMPEL
    formatierterCode += "<h2>LEDAmpel</h2>";
    formatierterCode += "<h3>Helligkeit</h3><p>";
    formatierterCode += ampelLichtstaerkeGruen;
    formatierterCode += "</p>";
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
  formatierterCode += "<h1>Adminseite</h1>";
  formatierterCode += "<h2>Variablen</h2><h3>LED Ampel</h3>";
  formatierterCode += "<h4>Helligkeit</h4>";
  formatierterCode += "<p>Schwellwert gruen: <input type=\"number\" name=\"ampelLichtstaerkeGruen\" placeholder=\"";
  formatierterCode += ampelLichtstaerkeGruen;
  formatierterCode += "\"></p>";
  formatierterCode += "<p>Schwellwert gelb: <input type=\"number\" name=\"ampelLichtstaerkeGelb\" placeholder=\"";
  formatierterCode += ampelLichtstaerkeGelb;
  formatierterCode += "\"></p>";
  formatierterCode += "<p>Schwellwert rot: <input type=\"number\" name=\"ampelLichtstaerkeRot\" placeholder=\"";
  formatierterCode += ampelLichtstaerkeRot;
  formatierterCode += "\"></p>";
  formatierterCode += "<h2>Passwort</h2>";
  formatierterCode += "<form action=\"/setzeVariablen\" method=\"POST\"><input type=\"password\" name=\"Passwort\" placeholder=\"Passwort\">";
  formatierterCode += "<input type=\"submit\" value=\"Login\"></form>";

  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
}

void WebseiteSetzeVariablen() {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von WebseiteSetzeVariablen()"));
    Serial.println(F("#######################################"));
  #endif
  if ( ! Webserver.hasArg("Passwort") || Webserver.arg("Passwort") == NULL) { // If the POST request doesn't have username and password data
    Webserver.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  }
  if(Webserver.arg("Passwort") == wifiAdminPasswort) { // If both the username and the password are correct
    ampelLichtstaerkeGruen = Webserver.arg("ampelLichtstaerkeGruen").toInt();
    ampelLichtstaerkeGelb = Webserver.arg("ampelLichtstaerkeGelb").toInt();
    ampelLichtstaerkeRot = Webserver.arg("ampelLichtstaerkeRot").toInt();
    #include "modul_wifi_header.h"
    #include "modul_wifi_footer.h"
    String formatierterCode = htmlHeader;
    formatierterCode += "<h2>Erfolgreich!</h2>";
    formatierterCode += htmlFooter;
    Webserver.send(200, "text/html", formatierterCode);
  } else {                                                                              // Username and password don't match
    Webserver.send(401, "text/plain", "401: Unauthorized");
  }
  #if MODUL_DEBUG
    Serial.print(F("Passwort = ")); Serial.println(F(wifiAdminPasswort));
    Serial.print(F("ampelLichtstaerkeGruen = ")); Serial.println(ampelLichtstaerkeGruen);
    Serial.print(F("ampelLichtstaerkeGelb = ")); Serial.println(ampelLichtstaerkeGelb);
    Serial.print(F("ampelLichtstaerkeRot = ")); Serial.println(ampelLichtstaerkeRot);
    Serial.println(F("#######################################"));
  #endif
}

/*
 * Funktion: WifiSetup()
 * Verbindet das WLAN
 */
void WifiSetup(String hostname){
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
  while (!(WiFi.status() == WL_CONNECTED) && i<30) {
      #if MODUL_DEBUG
        Serial.print("Verbindungsversuch ");
        Serial.print(i);
        Serial.println(" von 30.");
        delay(1000);
      #endif
      i++;
  }
  if (WiFi.status() != WL_CONNECTED) Serial.println("Keine WLAN-Verbindung möglich.");
  // Nun sollte WLAN verbunden sein
  Serial.print("meine IP: ");
  Serial.println(WiFi.localIP());
  // DNS Namensauflösung aktivieren:
  if (MDNS.begin(hostname)) {
    Serial.print("Gerät unter ");
    Serial.print(hostname);
    Serial.println(" erreichbar.");
  } else {
    Serial.println("Fehler bein Einrichten der Namensauflösung.");
  }
  Webserver.on("/", HTTP_GET, WebseiteStartAusgeben);
  Webserver.on("/admin.html", HTTP_GET, WebseiteAdminAusgeben);
  Webserver.on("/setzeVariablen", HTTP_POST, WebseiteSetzeVariablen);
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
