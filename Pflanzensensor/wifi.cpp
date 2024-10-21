/**
 * @file wifi.cpp
 * @brief Implementierung des WiFi-Moduls und Webservers für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul implementiert Funktionen zur WiFi-Verbindung und
 * zur Steuerung des integrierten Webservers.
 */

#include <ArduinoJson.h>
#include "einstellungen.h"
#include "passwoerter.h"
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

bool wlanAenderungVorgenommen = false;
ESP8266WiFiMulti wifiMulti;
ESP8266WebServer Webserver(80); // Webserver auf Port 80


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
    wifiMulti.addAP(wifiSsid1.c_str(), wifiPasswort1.c_str()); // WLAN Verbindung konfigurieren
    wifiMulti.addAP(wifiSsid2.c_str(), wifiPasswort2.c_str());
    wifiMulti.addAP(wifiSsid3.c_str(), wifiPasswort3.c_str());
    if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
      ip = WiFi.localIP().toString(); // IP Adresse in Variable schreiben
      logger.info(F(" .. WLAN verbunden: "));
      logger.info(F("SSID: ") + String(WiFi.SSID()));
      logger.info(F("IP: ") + String(ip));
      #if MODUL_DISPLAY
        DisplaySechsZeilen(F("WLAN OK"), F(""), F("SSID: ") + WiFi.SSID(), F("IP: ") + ip, F(" "), F(" "));
        delay(5000); // genug Zeit um die IP Adresse zu lesen
      #endif
    } else {
      logger.error(F(" .. Fehler: WLAN Verbindungsfehler!"));
      #if MODUL_DISPLAY
        DisplayDreiWoerter(F("WLAN"), F("Verbindungs-"), F("fehler!"));
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
    ip = WiFi.softAPIP().toString(); // IP Adresse in Variable schreiben
    logger.info(F(" .. Accesspoint wurde "));
    if( !result ) { // falls der Accesspoint nicht erfolgreich aufgebaut wurde
      logger.info(F("     NICHT "));
      #if MODUL_DISPLAY
        DisplayDreiWoerter(F("Acesspoint:"),F("Fehler beim"), F("Setup!"));
      #endif
    } else { // falls der Accesspoint erfolgreich aufgebaut wurde
      #if MODUL_DISPLAY
        if ( wifiApPasswortAktiviert) {
          DisplaySechsZeilen(F("Accesspoint OK"), F("SSID: ") + wifiApSsid, F("PW:") + wifiApPasswort, F("IP: ") + String(ip), F(" "), F(" "));
        } else {
          DisplaySechsZeilen(F("Accesspoint OK"), F("SSID: ") + wifiApSsid, F("PW: ohne"), F("IP: ") + String(ip), F(" "), F(" "));
        }
      #endif
    }
    logger.info(F("       erfolgreich aufgebaut!"));
    logger.info(F(" .. meine IP: ") + String(ip)); // IP Adresse ausgeben
  }

  // Verwende Funktionszeiger für Webserver-Routen
  using HandlerFunction = std::function<void(void)>;
  const HandlerFunction handlers[] = {
    WebseiteStartAusgeben,
    WebseiteAdminAusgeben,
    WebseiteDebugAusgeben,
    WebseiteSetzeVariablen,
    LeseMesswerte
  };
  const char* paths[] = {
    "/",
    "/admin.html",
    "/debug.html",
    "/setzeVariablen",
    "/leseMesswerte"
  };

  for (int i = 0; i < 5; i++) {
    Webserver.on(paths[i], HTTP_ANY, handlers[i]);
  }

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
        Webserver.send(404, F("text/plain"), F("Bild nicht gefunden"));
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
    Webserver.streamFile(css, F("text/css"));
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
    ip = WiFi.softAPIP().toString();

    logger.info(F("Access Point gestartet. IP: ") + String(ip));

    #if MODUL_DISPLAY
      DisplaySechsZeilen(F("AP-Modus"), F("aktiv"), F("SSID: ") + String(wifiApSsid), F("IP: ") + ip, F(" "), wifiHostname);
    #endif
  } else {
    // Versuche, sich mit konfiguriertem WLAN zu verbinden
    logger.info(F("Versuche, WLAN-Verbindung herzustellen..."));
    WiFi.mode(WIFI_STA);
    wifiMulti.cleanAPlist();

    // Fügt die konfigurierten WLANs hinzu
    wifiMulti.addAP(wifiSsid1.c_str(), wifiPasswort1.c_str());
    wifiMulti.addAP(wifiSsid2.c_str(), wifiPasswort2.c_str());
    wifiMulti.addAP(wifiSsid3.c_str(), wifiPasswort3.c_str());

    #if MODUL_DISPLAY
      DisplayDreiWoerter(F("Neustart"), F("WLAN"), F("Modul"));
    #endif

    // Versucht, eine Verbindung herzustellen
    if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
      ip = WiFi.localIP().toString();
      logger.info(F("Verbunden mit WLAN. IP: ") + String(ip));

      #if MODUL_DISPLAY
        DisplaySechsZeilen(F("WLAN OK"), F(""), F("SSID: ") + WiFi.SSID(), F("IP: ") + String(ip), F(" "), F(" "));
      #endif
    } else {
      // Wenn keine Verbindung möglich ist, wechsle in den AP-Modus
      logger.warning(F("Konnte keine WLAN-Verbindung herstellen. Wechsle in den AP-Modus."));
      wifiAp = true;
      WiFi.mode(WIFI_AP);
      WiFi.softAP(wifiApSsid, wifiApPasswort);
      ip = WiFi.softAPIP().toString();

      logger.info(F("AP-Modus aktiviert. IP: ") + String(ip));

      #if MODUL_DISPLAY
        DisplaySechsZeilen(F("AP-Modus"), F("aktiv"), F("SSID: ") + String(wifiApSsid), F("IP: ") + String(ip), F(" "), F(" "));
      #endif
    }
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
  JsonDocument doc;  // Verwende JsonDocument statt StaticJsonDocument

  #if MODUL_BODENFEUCHTE
    doc["bodenfeuchte"] = bodenfeuchteMesswert;
  #endif

  #if MODUL_HELLIGKEIT
    doc["helligkeit"] = helligkeitMesswert;
  #endif

  #if MODUL_DHT
    doc["lufttemperatur"] = lufttemperaturMesswert;
    doc["luftfeuchte"] = luftfeuchteMesswert;
  #endif

  #if MODUL_ANALOG3
    doc["analog3"] = analog3Messwert;
  #endif

  #if MODUL_ANALOG4
    doc["analog4"] = analog4Messwert;
  #endif

  #if MODUL_ANALOG5
    doc["analog5"] = analog5Messwert;
  #endif

  #if MODUL_ANALOG6
    doc["analog6"] = analog6Messwert;
  #endif

  #if MODUL_ANALOG7
    doc["analog7"] = analog7Messwert;
  #endif

  #if MODUL_ANALOG8
    doc["analog8"] = analog8Messwert;
  #endif

  String json;
  serializeJson(doc, json);

  Webserver.send(200, "application/json", json);
}

