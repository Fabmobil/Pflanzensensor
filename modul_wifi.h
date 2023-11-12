/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul
 */

#include <ESP8266WiFi.h> // für WLAN
#include <ESP8266WebServer.h> // für Webserver
#include <FS.h> // um die HTML-Dateien zu lesen

WiFiClient client;
ESP8266WebServer server(80); 

String FormatiereHtml(const char* dateiname) {
  // Versuche, die HTML-Datei zu öffnen
  File datei = SPIFFS.open(dateiname, "r");
  if (!datei) {
    Serial.println("Fehler beim Öffnen der Datei");
    return "";
  }

  // Lese den Inhalt der HTML-Datei
  String htmlText = datei.readString();
  datei.close();

  // Ersetze Anführungszeichen für die Verwendung im F() - Makro
  htmlText.replace("\"", "\\\"");

  // Erstelle den formatierten Code
  String formatierterCode = "String formatierterCode = F(\"";
  formatierterCode += htmlText;
  formatierterCode += "\");";

  return formatierterCode;
}

/*
 * Funktion: String WebseiteKopfAusgeben()
 * Liest die /HTML/header.html Datei aus, formatiert sie um
 * und gibt sie als String zurück.
 */
String WebseiteKopfAusgeben() {
  // Initialisiere das Dateisystem
  if (!SPIFFS.begin()) {
    Serial.println("Fehler beim Initialisieren des Dateisystems");
    return "Fehler beim Initialisieren des Dateisystems";
  }
  // Gib den formatierten Code für die HTML-Datei aus
  String formatierterCode = F("<!DOCTYPE html>");
  formatierterCode += FormatiereHtml("/HTML/header.html");
  return formatierterCode;
}

/*
 * Funktion: Void WebseiteStartAusgeben()
 * Gibt die Startseite des Webservers aus.
 */
void WebseiteStartAusgeben() {
  String formatierterCode = WebseiteKopfAusgeben();
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
  formatierterCode += "</div></body></html>";
  server.send(200, "text/html", formatierterCode);
}

void WebseiteAdminAusgeben() {
  String formatierterCode = WebseiteKopfAusgeben();
  formatierterCode += "<h2>Adminseite</h2>"
  formatierterCode += "</div></body></html>";
  server.send(200, "text/html", formatierterCode);
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
  server.on("/", WebseiteStartAusgeben);
  server.begin(); // Webserver starten
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
