/**
 * @file wifi.h
 * @brief WiFi-Modul und Webserver für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur WiFi-Verbindung und
 * zur Steuerung des integrierten Webservers.
 */

#ifndef WIFI_H
#define WIFI_H

bool wlanAenderungVorgenommen = false;

void WebseiteBild(const char* pfad, const char* mimeType);
void WebseiteCss();
void NeustartWLANVerbindung();
void VerzoegerterWLANNeustart();

#include <ESP8266WiFiMulti.h> // für WLAN
ESP8266WiFiMulti wifiMulti;
#include <ESP8266WebServer.h> // für Webserver
#include <ESP8266mDNS.h> // für Namensauflösung

ESP8266WebServer Webserver(80); // Webserver auf Port 80
#include "wifi_footer.h" // Kopf der HTML-Seite
#include "wifi_header.h"// Fuß der HTML-Seite
#include "wifi_seite_admin.h" // für die Administrationsseite
#include "wifi_seite_debug.h" // für die Debugseite
#include "wifi_seite_nichtGefunden.h" // für die Seite, die angezeigt wird, wenn die angefragte Seite nicht gefunden wurde
#include "wifi_seite_start.h" // für die Startseite
#include "wifi_seite_setzeVariablen.h" // für das Setzen der Variablen



/**
 * @brief Initialisiert die WiFi-Verbindung
 *
 * Diese Funktion stellt eine WiFi-Verbindung her oder erstellt einen Access Point,
 * je nach Konfiguration.
 *
 * @param hostname Der zu verwendende Hostname für das Gerät
 * @return String Die IP-Adresse des Geräts
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
    wifiMulti.addAP(wifiSsid1.c_str(), wifiPassword1.c_str()); // WLAN Verbindung konfigurieren
    wifiMulti.addAP(wifiSsid2.c_str(), wifiPassword2.c_str());
    wifiMulti.addAP(wifiSsid3.c_str(), wifiPassword3.c_str());
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
  Webserver.on("/Bilder/logoFabmobil.png", HTTP_GET, []() {
      WebseiteBild("/Bilder/logoFabmobil.png", "image/png");
  });
  Webserver.on("/Bilder/logoGithub.png", HTTP_GET, []() {
      WebseiteBild("/Bilder/logoGithub.png", "image/png");
  });
  Webserver.on("/favicon.ico", HTTP_GET, []() {
      WebseiteBild("/favicon.ico", "image/x-icon");
  });
  Webserver.on("/style.css", HTTP_GET, WebseiteCss);

  Webserver.onNotFound(WebseiteNichtGefundenAusgeben);
  Webserver.begin(); // Webserver starten
  return ip; // IP Adresse zurückgeben
}

/**
 * @brief Sendet ein Bild über den Webserver
 *
 * @param pfad Der Dateipfad des Bildes
 * @param mimeType Der MIME-Typ des Bildes
 */
void WebseiteBild(const char* pfad, const char* mimeType) {
    File bild = LittleFS.open(pfad, "r");
    if (!bild) {
        Serial.print("Fehler: ");
        Serial.print(pfad);
        Serial.println(" konnte nicht geöffnet werden!");
        Webserver.send(404, "text/plain", "Bild nicht gefunden");
        return;
    }
    Webserver.streamFile(bild, mimeType);
    bild.close();
}

/**
 * @brief Sendet die CSS-Datei über den Webserver
 */
void WebseiteCss() {
    if (!LittleFS.exists("/style.css")) {
        Serial.println("Fehler: /style.css existiert nicht!");
        return;
    }
    File css = LittleFS.open("/style.css", "r");
    if (!css) {
        Serial.println("Fehler: /style.css kann nicht geöffnet werden!");
        return;
    }
    Webserver.streamFile(css, "text/css");
    css.close();
}

/**
 * @brief Startet die WLAN-Verbindung neu
 *
 * Diese Funktion überprüft den WLAN-Modus und stellt entweder einen Access Point her
 * oder versucht, sich mit konfigurierten WLANs zu verbinden. Wenn keine Verbindung
 * möglich ist, wird automatisch in den Access Point Modus gewechselt.
 */
void NeustartWLANVerbindung() {
  WiFi.disconnect();  // Trennt die bestehende Verbindung
  Serial.println("wifiAp: " + String(wifiAp));
  if (wifiAp) {
    // Access Point Modus
    Serial.println(F("Starte Access Point Modus..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiApSsid, wifiApPasswort);
    ip = WiFi.softAPIP().toString();

    Serial.print(F("Access Point gestartet. IP: "));
    Serial.println(ip);

    #if MODUL_DISPLAY
      DisplaySechsZeilen("AP-Modus", "aktiv", "SSID: " + String(wifiApSsid), "IP: " + ip, "Hostname:", wifiHostname + ".local");
    #endif
  } else {
    // Versuche, sich mit konfiguriertem WLAN zu verbinden
    Serial.println(F("Versuche, WLAN-Verbindung herzustellen..."));
    WiFi.mode(WIFI_STA);
    wifiMulti.cleanAPlist();

    // Fügt die konfigurierten WLANs hinzu
    wifiMulti.addAP(wifiSsid1.c_str(), wifiPassword1.c_str());
    wifiMulti.addAP(wifiSsid2.c_str(), wifiPassword2.c_str());
    wifiMulti.addAP(wifiSsid3.c_str(), wifiPassword3.c_str());

    #if MODUL_DISPLAY
      DisplayDreiWoerter("Verbinde", "mit", "WLAN...");
    #endif

    // Versucht, eine Verbindung herzustellen
    if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
      ip = WiFi.localIP().toString();
      Serial.print(F("Verbunden mit WLAN. IP: "));
      Serial.println(ip);

      #if MODUL_DISPLAY
        DisplaySechsZeilen("WLAN OK", "", "SSID: " + WiFi.SSID(), "IP: "+ ip, "Hostname:", wifiHostname + ".local");
      #endif
    } else {
      // Wenn keine Verbindung möglich ist, wechsle in den AP-Modus
      Serial.println(F("Konnte keine WLAN-Verbindung herstellen. Wechsle in den AP-Modus."));
      wifiAp = true;
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wifiApSsid, wifiApPasswort);
      ip = WiFi.softAPIP().toString();

      Serial.print(F("AP-Modus aktiviert. IP: "));
      Serial.println(ip);

      #if MODUL_DISPLAY
        DisplaySechsZeilen("AP-Modus", "aktiv", "SSID: " + String(wifiApSsid), "IP: " + ip, "Hostname:", wifiHostname + ".local");
      #endif
    }
  }

  // DNS-Server neu starten
  if (MDNS.begin(wifiHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
}


/**
 * @brief Plant einen verzögerten Neustart der WLAN-Verbindung
 *
 * Diese Funktion setzt einen Timer für einen zukünftigen WLAN-Neustart.
 */
void VerzoegerterWLANNeustart() {
  geplantesWLANNeustartZeit = millis() + 10000; // Plant den Neustart in 5 Sekunden
  wlanNeustartGeplant = true;
}


#endif // WIFI_H
