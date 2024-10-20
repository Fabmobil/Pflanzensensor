/**
 * @file wifi.cpp
 * @brief Implementierung des WiFi-Moduls und Webservers für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul implementiert Funktionen zur WiFi-Verbindung und
 * zur Steuerung des integrierten Webservers.
 */

#include "wifi.h"
#include "wifi_footer.h"
#include "wifi_header.h"
#include "wifi_seite_admin.h"
#include "wifi_seite_debug.h"
#include "wifi_seite_nichtGefunden.h"
#include "wifi_seite_start.h"
#include "wifi_seite_setzeVariablen.h"
#if MODUL_DISPLAY
  #include "display.h"
#endif
#include <time.h>

bool wlanAenderungVorgenommen = false;
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer Webserver(80); // Webserver auf Port 80
extern bool wlanNeustartGeplant;
extern unsigned long geplanteWLANNeustartZeit;
extern char wifiApPasswort[35];
extern char wifiHostname[20];
extern char wifiApSsid[40];
extern bool wifiApPasswortAktiviert;

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
logger.debug(F("Beginn von WifiSetup()"));

// WLAN Verbindung herstellen
  WiFi.mode(WIFI_OFF); // WLAN ausschalten
  if ( !wifiAp ) { // falls kein eigener Accesspoint aufgemacht werden soll wird sich mit dem definierten WLAN verbunden
    WiFi.mode(WIFI_STA);; // WLAN im Clientmodus starten
    // Wifi-Verbindungen konfigurieren
    wifiMulti.addAP(wifiSsid1, wifiPasswort1); // WLAN Verbindung konfigurieren
    wifiMulti.addAP(wifiSsid2, wifiPasswort2);
    wifiMulti.addAP(wifiSsid3, wifiPasswort3);
    if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
      String tempString = WiFi.localIP().toString();
      strncpy(ip, tempString.c_str(), sizeof(ip) - 1);
      ip[sizeof(ip) - 1] = '\0';// IP Adresse in Variable schreiben
      logger.info(F(" .. WLAN verbunden: "));
      logger.info(F("SSID: ") + String(WiFi.SSID()));
      logger.info(F("IP: ") + String(ip));
      #if MODUL_DISPLAY
        DisplaySechsZeilen("WLAN OK", "", "SSID: " + String(WiFi.SSID()), "IP: "+ String(ip), "Hostname: ", "  " + String(hostname) + ".local" );
        delay(5000); // genug Zeit um die IP Adresse zu lesen
      #endif
      // Zeit synchronisieren
      configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
      logger.info(F("Warte auf die Synchronisation von Uhrzeit und Datum: "));
      time_t now = time(nullptr);
      while (now < 8 * 3600 * 2) {
        delay(500);
        logger.debug(F("."));
        now = time(nullptr);
      }
      struct tm timeinfo;
      gmtime_r(&now, &timeinfo);
      logger.info(F("Die Zeit und das Datum ist: ") + String(asctime(&timeinfo)));
    } else {
      logger.error(F(" .. Fehler: WLAN Verbindungsfehler!"));
      #if MODUL_DISPLAY
        DisplayDreiWoerter("WLAN", "Verbindungs-", "fehler!");
      #endif
    }
  } else { // ansonsten wird hier das WLAN erstellt
    logger.info(F("Konfiguriere soft-AP ... "));
    boolean result = false; // Variable für den Erfolg des Aufbaus des Accesspoints
    if ( wifiApPasswortAktiviert ) { // Falls ein WLAN mit Passwort erstellt werden soll
      result = WiFi.softAP(wifiApSsid, wifiApPasswort ); // WLAN mit Passwort erstellen
    } else { // ansonsten WLAN ohne Passwort
      result = WiFi.softAP(wifiApSsid); // WLAN ohne Passwort erstellen
    }
    String tempString = WiFi.softAPIP().toString();
    strncpy(ip, tempString.c_str(), sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = '\0'; // IP Adresse in Variable schreiben
    logger.info(F(" .. Accesspoint wurde "));
    if( !result ) { // falls der Accesspoint nicht erfolgreich aufgebaut wurde
      logger.info(F("     NICHT "));
      #if MODUL_DISPLAY
        DisplayDreiWoerter("Acesspoint:","Fehler beim", "Setup!");
      #endif
    } else { // falls der Accesspoint erfolgreich aufgebaut wurde
      #if MODUL_DISPLAY
        if ( wifiApPasswortAktiviert) {
          DisplaySechsZeilen("Accesspoint OK", "SSID: " + String(wifiApSsid), "PW:" + String(wifiApPasswort), "IP: "+ String(ip), "Hostname: ", String(hostname) + ".local" );
        } else {
          DisplaySechsZeilen("Accesspoint OK", "SSID: " + String(wifiApSsid), "PW: ohne", "IP: "+ String(ip), "Hostname: ", String(hostname)   + ".local" );
        }
      #endif
    }
    logger.info(F("       erfolgreich aufgebaut!"));
    logger.info(F(" .. meine IP: ") + String(ip)); // IP Adresse ausgeben
  }

  // DNS Namensauflösung aktivieren:
  if (MDNS.begin(hostname)) { // falls Namensauflösung erfolgreich eingerichtet wurde
    logger.info(F(" .. Gerät unter ") + String(hostname) + F(".local erreichbar."));
    MDNS.addService("http", "tcp", 80); // Webserver unter Port 80 bekannt machen
  } else { // falls Namensauflösung nicht erfolgreich eingerichtet wurde
    logger.error(F(" .. Fehler bein Einrichten der Namensauflösung."));
  }
  Webserver.on("/", HTTP_GET, WebseiteStartAusgeben);
  Webserver.on("/admin.html", HTTP_GET, WebseiteAdminAusgeben);
  Webserver.on("/debug.html", HTTP_GET, WebseiteDebugAusgeben);
  Webserver.on("/setzeVariablen", HTTP_POST, WebseiteSetzeVariablen);
  Webserver.on("/leseMesswerte", HTTP_GET, LeseMesswerte);

  Webserver.on("/neuesteLogs", HTTP_GET, []() {
    String logs = logger.getLogsAsHtmlTable();
    Webserver.send(200, "text/html", logs);
  });
  Webserver.on("/downloadLog", DownloadLog);
  Webserver.on("/SetzeLogLevel", HTTP_POST, SetzeLogLevel);
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
        logger.error(F("Fehler: ") + String(pfad) + F(" konnte nicht geöffnet werden!"));
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
        logger.error(F("Fehler: /style.css existiert nicht!"));
        return;
    }
    File css = LittleFS.open("/style.css", "r");
    if (!css) {
        logger.error(F("Fehler: /style.css kann nicht geöffnet werden!"));
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
  logger.info(F("wifiAp: ") + String(wifiAp));
  if (wifiAp) {
    // Access Point Modus
    logger.info(F("Starte Access Point Modus..."));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(wifiApSsid, wifiApPasswort);
    String tempString = WiFi.softAPIP().toString();
    strncpy(ip, tempString.c_str(), sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = '\0';

    logger.info(F("Access Point gestartet. IP: ") + String(ip));

    #if MODUL_DISPLAY
      DisplaySechsZeilen("AP-Modus", "aktiv", "SSID: " + String(wifiApSsid), "IP: " + String(ip), "Hostname:", String(wifiHostname) + ".local");
    #endif
  } else {
    // Versuche, sich mit konfiguriertem WLAN zu verbinden
    logger.info(F("Versuche, WLAN-Verbindung herzustellen..."));
    WiFi.mode(WIFI_STA);
    wifiMulti.cleanAPlist();

    // Fügt die konfigurierten WLANs hinzu
    wifiMulti.addAP(wifiSsid1, wifiPasswort1);
    wifiMulti.addAP(wifiSsid2, wifiPasswort2);
    wifiMulti.addAP(wifiSsid3, wifiPasswort3);

    #if MODUL_DISPLAY
      DisplayDreiWoerter("Neustart", "WLAN", "Modul");
    #endif

    // Versucht, eine Verbindung herzustellen
    if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
      String tempString = WiFi.localIP().toString();
      strncpy(ip, tempString.c_str(), sizeof(ip) - 1);
      ip[sizeof(ip) - 1] = '\0';
      logger.info(F("Verbunden mit WLAN. IP: ") + String(ip));

      #if MODUL_DISPLAY
        DisplaySechsZeilen("WLAN OK", "", "SSID: " + WiFi.SSID(), "IP: "+ String(ip), "Hostname:", String(wifiHostname) + ".local");
      #endif
    } else {
      // Wenn keine Verbindung möglich ist, wechsle in den AP-Modus
      logger.warning(F("Konnte keine WLAN-Verbindung herstellen. Wechsle in den AP-Modus."));
      wifiAp = true;
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wifiApSsid, wifiApPasswort);
      String tempString = WiFi.softAPIP().toString();
      strncpy(ip, tempString.c_str(), sizeof(ip) - 1);
      ip[sizeof(ip) - 1] = '\0';

      logger.info(F("AP-Modus aktiviert. IP: ") + String(ip));

      #if MODUL_DISPLAY
        DisplaySechsZeilen("AP-Modus", "aktiv", "SSID: " + String(wifiApSsid), "IP: " + String(ip), "Hostname:", String(wifiHostname) + ".local");
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
  geplanteWLANNeustartZeit = millis() + 10000; // Plant den Neustart in 5 Sekunden
  wlanNeustartGeplant = true;
}


void SetzeLogLevel() {
  String level = Webserver.arg("logLevel");
  if (level == "DEBUG") {
    logger.setLogLevel(LogLevel::DEBUG);
  } else if (level == "INFO") {
    logger.setLogLevel(LogLevel::INFO);
  } else if (level == "WARNING") {
    logger.setLogLevel(LogLevel::WARNING);
  } else if (level == "ERROR") {
    logger.setLogLevel(LogLevel::ERROR);
  }
  Webserver.send(200, "text/html", logger.getLogsAsHtmlTable(logAnzahlWebseite));;
}

void DownloadLog() {
  if (!logger.isFileLoggingEnabled()) {
    Webserver.send(403, "text/plain", "File logging is disabled");
    return;
  }

  String logContent = logger.getLogFileContent();
  Webserver.sendHeader("Content-Disposition", "attachment; filename=system.log");
  Webserver.send(200, "text/plain", logContent);
}

void LeseMesswerte() {
  String json = "{";
  #if MODUL_BODENFEUCHTE
    json += "\"bodenfeuchte\":" + String(bodenfeuchteMesswert) + ",";
  #endif

  #if MODUL_HELLIGKEIT
    json += "\"helligkeit\":" + String(helligkeitMesswert) + ",";
  #endif

  #if MODUL_DHT
    json += "\"lufttemperatur\":" + String(lufttemperaturMesswert) + ",";
    json += "\"luftfeuchte\":" + String(luftfeuchteMesswert) + ",";
  #endif

  #if MODUL_ANALOG3
    json += "\"analog3\":" + String(analog3Messwert) + ",";
  #endif

  #if MODUL_ANALOG4
    json += "\"analog4\":" + String(analog4Messwert) + ",";
  #endif

  #if MODUL_ANALOG5
    json += "\"analog5\":" + String(analog5Messwert) + ",";
  #endif

  #if MODUL_ANALOG6
    json += "\"analog6\":" + String(analog6Messwert) + ",";
  #endif

  #if MODUL_ANALOG7
    json += "\"analog7\":" + String(analog7Messwert) + ",";
  #endif

  #if MODUL_ANALOG8
    json += "\"analog8\":" + String(analog8Messwert) + ",";
  #endif

  // Entferne das letzte Komma, falls vorhanden
  if (json.endsWith(",")) {
    json.remove(json.length() - 1);
  }

  json += "}";

  // Sende die JSON-Antwort an den Client
  Webserver.send(200, "application/json", json);
}
