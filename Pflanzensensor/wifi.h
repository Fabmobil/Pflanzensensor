/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul und den Webserver
 */

#include <ESP8266WiFi.h> // für WLAN
#include <ESP8266WebServer.h> // für Webserver
#include <ESP8266mDNS.h> // für Namensauflösung

WiFiClient client;
ESP8266WebServer Webserver(80); // Webserver auf Port 80

#include "wifi_seite_admin.h" // für die Administrationsseite
#include "wifi_seite_debug.h" // für die Debugseite
#include "wifi_seite_start.h" // für die Startseite
#include "wifi_seite_setzeVariablen.h" // für das Setzen der Variablen

/*
 * Funktion: WifiSetup()
 * Verbindet das WLAN
 */
String WifiSetup(String hostname){
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WifiSetup()"));
  #endif
// WLAN Verbindung herstellen
  WiFi.mode(WIFI_OFF); // WLAN ausschalten
  String ip = "keine WLAN Verbindung."; // Initialisierung der IP Adresse mit Fehlermeldung
  if ( !wifiAp ) { // falls kein eigener Accesspoint aufgemacht werden soll wird sich mit dem definierten WLAN verbunden
    WiFi.mode(WIFI_AP_STA); // WLAN als Client und Accesspoint
    if (WiFi.status() == WL_CONNECTED) { // falls WLAN bereits verbunden ist
      Serial.println("WLAN war verbunden");
    }
    WiFi.begin(wifiSsid, wifiPassword); // WLAN Verbindung herstellen
    int i=0; // Es wird nur 30 mal versucht, eine WLAN Verbindung aufzubauen
    Serial.print("WLAN-Verbindungsversuch: ");
    while (!(WiFi.status() == WL_CONNECTED) && i<30) { // solange keine WLAN Verbindung besteht und i kleiner als 30 ist
        Serial.print(i);
        Serial.println(" von 30.");
        delay(1000); // 1 Sekunde warten
        i++; // i um 1 erhöhen
    }
    if (WiFi.status() != WL_CONNECTED) { // falls nach 30 Versuchen keine WLAN Verbindung besteht
      Serial.println("Keine WLAN-Verbindung möglich.");
    }
    // Nun sollte WLAN verbunden sein
    Serial.print("meine IP: "); // IP Adresse ausgeben
    Serial.println(WiFi.localIP());
    ip = WiFi.localIP().toString(); // IP Adresse in Variable schreiben
  } else { // ansonsten wird hier das WLAN erstellt
    Serial.print("Konfiguriere soft-AP ... ");
    boolean result = false; // Variable für den Erfolg des Aufbaus des Accesspoints
    if ( wifiApPasswortAktiviert ) { // Falls ein WLAN mit Passwort erstellt werden soll
      result = WiFi.softAP(wifiApSsid, wifiApPasswort ); // WLAN mit Passwort erstellen
    } else { // ansonsten WLAN ohne Passwort
      result = WiFi.softAP(wifiApSsid); // WLAN ohne Passwort erstellen
    }
    Serial.print(F("Accesspoint wurde "));
    if( !result ) { // falls der Accesspoint nicht erfolgreich aufgebaut wurde
      Serial.println(F("NICHT "));
    }
    Serial.println(F("erfolgreich aufgebaut!"));
    Serial.print("meine IP: ");
    Serial.println(WiFi.softAPIP()); // IP Adresse ausgeben
    ip = WiFi.softAPIP().toString(); // IP Adresse in Variable schreiben
  }

  // DNS Namensauflösung aktivieren:
  if (MDNS.begin(hostname)) { // falls Namensauflösung erfolgreich eingerichtet wurde
    Serial.print("Gerät unter ");
    Serial.print(hostname);
    Serial.println(".local erreichbar.");
    MDNS.addService("http", "tcp", 80); // Webserver unter Port 80 bekannt machen
  } else { // falls Namensauflösung nicht erfolgreich eingerichtet wurde
    Serial.println("Fehler bein Einrichten der Namensauflösung.");
  }
  Webserver.on("/", HTTP_GET, WebseiteStartAusgeben);
  Webserver.on("/admin.html", HTTP_GET, WebseiteAdminAusgeben);
  Webserver.on("/debug.html", HTTP_GET, WebseiteDebugAusgeben);
  Webserver.on("/setzeVariablen", HTTP_POST, WebseiteSetzeVariablen);
  Webserver.begin(); // Webserver starten
  return ip; // IP Adresse zurückgeben
}
