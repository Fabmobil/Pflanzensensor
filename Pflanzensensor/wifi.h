/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul und den Webserver
 */

#include <ESP8266WiFiMulti.h> // für WLAN
ESP8266WiFiMulti wifiMulti;
#include <ESP8266WebServer.h> // für Webserver
#include <ESP8266mDNS.h> // für Namensauflösung

ESP8266WebServer Webserver(80); // Webserver auf Port 80
#include "wifi_footer.h" // Kopf der HTML-Seite
#include "wifi_header.h"// Fuß der HTML-Seite
#include "wifi_daten.h" // Bilder die auf der Seite verwendet werden
#include "wifi_seite_admin.h" // für die Administrationsseite
#include "wifi_seite_debug.h" // für die Debugseite
#include "wifi_seite_nichtGefunden.h" // für die Seite, die angezeigt wird, wenn die angefragte Seite nicht gefunden wurde
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
  if ( !wifiAp ) { // falls kein eigener Accesspoint aufgemacht werden soll wird sich mit dem definierten WLAN verbunden
    WiFi.mode(WIFI_STA);; // WLAN im Clientmodus starten
    // Wifi-Verbindungen konfigurieren
    wifiMulti.addAP(wifiSsid1, wifiPassword1); // WLAN Verbindung konfigurieren
    wifiMulti.addAP(wifiSsid2, wifiPassword2);
    wifiMulti.addAP(wifiSsid3, wifiPassword3);
    if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
      Serial.print(" .. WLAN verbunden: ");
      Serial.print(WiFi.SSID());
      Serial.print(" ");
      Serial.println(WiFi.localIP());
      ip = WiFi.localIP().toString(); // IP Adresse in Variable schreiben
      #if MODUL_DISPLAY
        DisplaySechsZeilen("WLAN OK", "", "SSID: " + WiFi.SSID(), "IP: "+ ip, "Hostname: ", "  " + hostname + ".local" );
        delay(5000); // genug Zeit um die IP Adresse zu lesen
      #endif
    } else {
      Serial.println(" .. Fehler: WLAN Verbindungsfehler!");
      #if MODUL_DISPLAY
        DisplayDreiWoerter("WLAN", "Verbindungs-", "fehler!");
      #endif
    }
  } else { // ansonsten wird hier das WLAN erstellt
    Serial.print("Konfiguriere soft-AP ... ");
    boolean result = false; // Variable für den Erfolg des Aufbaus des Accesspoints
    if ( wifiApPasswortAktiviert ) { // Falls ein WLAN mit Passwort erstellt werden soll
      result = WiFi.softAP(wifiApSsid, wifiApPasswort ); // WLAN mit Passwort erstellen
    } else { // ansonsten WLAN ohne Passwort
      result = WiFi.softAP(wifiApSsid); // WLAN ohne Passwort erstellen
    }
    ip = WiFi.softAPIP().toString(); // IP Adresse in Variable schreiben
    Serial.print(F(" .. Accesspoint wurde "));
    if( !result ) { // falls der Accesspoint nicht erfolgreich aufgebaut wurde
      Serial.println(F("NICHT "));
      #if MODUL_DISPLAY
        DisplayDreiWoerter("Acesspoint:","Fehler beim", "Setup!");
      #endif
    } else { // falls der Accesspoint erfolgreich aufgebaut wurde
      #if MODUL_DISPLAY
        if ( wifiApPasswortAktiviert) {
          DisplaySechsZeilen("Accesspoint OK", "SSID: " + wifiApSsid, "PW:" + wifiApPasswort, "IP: "+ ip, "Hostname: ", hostname + ".local" );
        } else {
          DisplaySechsZeilen("Accesspoint OK", "SSID: " + wifiApSsid, "PW: ohne", "IP: "+ ip, "Hostname: ", hostname + ".local" );
        }
      #endif
    }
    Serial.println(F("erfolgreich aufgebaut!"));
    Serial.print(" .. meine IP: ");
    Serial.println(ip); // IP Adresse ausgeben
  }

  // DNS Namensauflösung aktivieren:
  if (MDNS.begin(hostname)) { // falls Namensauflösung erfolgreich eingerichtet wurde
    Serial.print(" .. Gerät unter ");
    Serial.print(hostname);
    Serial.println(".local erreichbar.");
    MDNS.addService("http", "tcp", 80); // Webserver unter Port 80 bekannt machen
  } else { // falls Namensauflösung nicht erfolgreich eingerichtet wurde
    Serial.println(" .. Fehler bein Einrichten der Namensauflösung.");
  }
  Webserver.on("/", HTTP_GET, WebseiteStartAusgeben);
  Webserver.on("/admin.html", HTTP_GET, WebseiteAdminAusgeben);
  Webserver.on("/debug.html", HTTP_GET, WebseiteDebugAusgeben);
  Webserver.on("/setzeVariablen", HTTP_POST, WebseiteSetzeVariablen);
  Webserver.on("/Bilder/logoFabmobil.png", HTTP_GET, WebseiteBildLogoFabmobil);
  Webserver.on("/Bilder/logoGithub.png", HTTP_GET, WebseiteBildLogoGithub);
  Webserver.on("/Bilder/hintergrund.png", HTTP_GET, WebseiteBildHintergrund);
  Webserver.on("/style.css", HTTP_GET, WebseiteCss);

  Webserver.onNotFound(WebseiteNichtGefundenAusgeben);
  Webserver.begin(); // Webserver starten
  return ip; // IP Adresse zurückgeben
}
