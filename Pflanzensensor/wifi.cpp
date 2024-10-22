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

RestartState restartState = RestartState::INIT;
unsigned long stateChangeMillis = 0;
bool restartInProgress = false;

String WifiSetup(String hostname) {
    logger.debug(F("Beginn von WifiSetup()"));

    WiFi.mode(WIFI_OFF);
    if (!wifiAp) {
        WiFi.mode(WIFI_STA);
        logger.info(F("Starte WLAN im Client-Modus"));

        // Konfigurierte WLANs ausgeben
        logger.info(F("Konfigurierte WLANs:"));
        if (!wifiSsid1.isEmpty()) {
            logger.info(F("WLAN 1 - SSID: ") + wifiSsid1 + F(", Passwort: ") + wifiPasswort1);
        } else {
            logger.info(F("WLAN 1 nicht konfiguriert"));
        }
        if (!wifiSsid2.isEmpty()) {
            logger.info(F("WLAN 2 - SSID: ") + wifiSsid2 + F(", Passwort: ") + wifiPasswort2);
        } else {
            logger.info(F("WLAN 2 nicht konfiguriert"));
        }
        if (!wifiSsid3.isEmpty()) {
            logger.info(F("WLAN 3 - SSID: ") + wifiSsid3 + F(", Passwort: ") + wifiPasswort3);
        } else {
            logger.info(F("WLAN 3 nicht konfiguriert"));
        }

        // WLAN-Status vor Verbindungsaufbau
        logger.info(F("Aktueller WLAN-Status: ") + String(WiFi.status()));
        logger.info(F("WLAN-Signalstärke: ") + String(WiFi.RSSI()) + F(" dBm"));

        wifiMulti.addAP(wifiSsid1.c_str(), wifiPasswort1.c_str());
        wifiMulti.addAP(wifiSsid2.c_str(), wifiPasswort2.c_str());
        wifiMulti.addAP(wifiSsid3.c_str(), wifiPasswort3.c_str());

        logger.info(F("Versuche Verbindung herzustellen (Timeout: ") + String(wifiTimeout) + F("ms)"));

        if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
            ip = WiFi.localIP().toString();
            logger.info(F("WLAN erfolgreich verbunden:"));
            logger.info(F("Verbundene SSID: ") + WiFi.SSID());
            logger.info(F("IP-Adresse: ") + ip);
            logger.info(F("Signalstärke: ") + String(WiFi.RSSI()) + F(" dBm"));
            #if MODUL_DISPLAY
                DisplaySechsZeilen(F("WLAN OK"), F(""), F("SSID: ") + WiFi.SSID(), F("IP: ") + ip, F(" "), F(" "));
                delay(5000);
            #endif
        } else {
            logger.error(F("WLAN-Verbindung fehlgeschlagen!"));
            logger.error(F("Letzter WLAN-Status: ") + String(WiFi.status()));

            #if MODUL_DISPLAY
                DisplayDreiWoerter(F("WLAN"), F("Verbindungs-"), F("fehler!"));
            #endif
        }
    } else {
        boolean result = wifiApPasswortAktiviert ? WiFi.softAP(wifiApSsid, wifiApPasswort) : WiFi.softAP(wifiApSsid);
        ip = WiFi.softAPIP().toString();
        logger.info(result ? F("Accesspoint erfolgreich aufgebaut!") : F("Accesspoint konnte NICHT aufgebaut werden!"));
        logger.info(F("SSID: ") + wifiApSsid );
        logger.info(F("IP: ") + ip);
        #if MODUL_DISPLAY
            if (result) {
                DisplaySechsZeilen(F("Accesspoint OK"), F("SSID: ") + wifiApSsid,
                                   wifiApPasswortAktiviert ? F("PW:") + wifiApPasswort : F("PW: ohne"),
                                   F("IP: ") + ip, F(" "), F(" "));
            } else {
                DisplayDreiWoerter(F("Accesspoint:"), F("Fehler beim"), F("Setup!"));
            }
        #endif
    }

    // Webserver-Routen
    Webserver.on("/", HTTP_ANY, WebseiteStartAusgeben);
    Webserver.on("/admin.html", HTTP_ANY, WebseiteAdminAusgeben);
    Webserver.on("/debug.html", HTTP_ANY, WebseiteDebugAusgeben);
    Webserver.on("/setzeVariablen", HTTP_ANY, WebseiteSetzeVariablen);
    Webserver.on("/leseMesswerte", HTTP_ANY, LeseMesswerte);
    Webserver.on("/neuesteLogs", HTTP_GET, []() {
        Webserver.send(200, "text/html", logger.LogsAlsHtmlTabelle());
    });
    Webserver.on("/downloadLog", HTTP_GET, DownloadLog);
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
    Webserver.begin();
    return ip;
}

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

void SetzeLogLevel() {
    String level = Webserver.arg("logLevel");
    if (level == "DEBUG") logger.SetzteLogLevel(LogLevel::DEBUG);
    else if (level == "INFO") logger.SetzteLogLevel(LogLevel::INFO);
    else if (level == "WARNING") logger.SetzteLogLevel(LogLevel::WARNING);
    else if (level == "ERROR") logger.SetzteLogLevel(LogLevel::ERROR);
    Webserver.send(200, "text/html", logger.LogsAlsHtmlTabelle(MAX_LOG_EINTRAEGE));
}

void DownloadLog() {
    if (!logger.IstLoggenInDateiAktiviert()) {
        Webserver.send(403, "text/plain", "File logging is disabled");
        return;
    }
    String logContent = logger.LogdateiInhaltAuslesen();
    Webserver.sendHeader("Content-Disposition", "attachment; filename=system.log");
    Webserver.send(200, "text/plain", logContent);
}

void LeseMesswerte() {
    JsonDocument doc;

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

void HandleRestart() {
    if (!restartInProgress) return;

    unsigned long currentMillis = millis();

    switch (restartState) {
        case RestartState::INIT:
            if (currentMillis - stateChangeMillis >= 1000) {
                VariablenSpeichern();
                stateChangeMillis = currentMillis;
                restartState = RestartState::SAVE_WAIT;
            }
            break;

        case RestartState::SAVE_WAIT:
            if (currentMillis - stateChangeMillis >= 1000) {
                Webserver.handleClient(); // Noch einmal Anfragen bearbeiten
                stateChangeMillis = currentMillis;
                restartState = RestartState::SEND_RESPONSE;
            }
            break;

        case RestartState::SEND_RESPONSE:
            if (currentMillis - stateChangeMillis >= 1000) {
                Webserver.handleClient(); // Letzte Anfragen bearbeiten
                stateChangeMillis = currentMillis;
                restartState = RestartState::FINAL_SAVE;
            }
            break;

        case RestartState::FINAL_SAVE:
            if (currentMillis - stateChangeMillis >= 1000) {
                Webserver.close();
                WiFi.disconnect(true);
                delay(1000);
                WiFi.mode(WIFI_OFF);
                delay(1000);
                stateChangeMillis = currentMillis;
                restartState = RestartState::RESTART;
            }
            break;

        case RestartState::RESTART:
            if (currentMillis - stateChangeMillis >= 1000) {
                logger.info(F("Führe Neustart durch..."));
                ESP.restart();
            }
            break;
    }
}
