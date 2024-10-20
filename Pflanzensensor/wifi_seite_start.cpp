/**
 * @file wifi_seite_start.cpp
 * @brief Implementierung der Startseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Implementierung der Funktionen zur Generierung der Startseite
 * des Pflanzensensors mit aktuellen Sensordaten.
 */

#include "wifi_seite_start.h"
#include "logger.h"
#include "einstellungen.h"
#include "passwoerter.h"
#include "wifi_header.h"
#include "wifi_footer.h"

#if MODUL_DHT
  extern float luftfeuchteMesswert;
  extern float lufttemperaturMesswert;
#endif

#if MODUL_BODENFEUCHTE
  extern int bodenfeuchteMesswertProzent;
#endif

#if MODUL_HELLIGKEIT
  extern int helligkeitMesswertProzent;
#endif

extern bool webhookAn;

void sendeSensorDaten(const String& sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit, bool alarm, bool webhook) {
  Webserver.sendContent(F("<h2>"));
  Webserver.sendContent(sensorName);
  if (alarm && webhook) {Webserver.sendContent(F(" ⏰"));}
  Webserver.sendContent(F("</h2>\n<div class=\""));
  Webserver.sendContent(sensorFarbe);
  Webserver.sendContent(F("\"><p>"));
  Webserver.sendContent(String(messwert));
  Webserver.sendContent(F(" "));
  Webserver.sendContent(einheit);
  Webserver.sendContent(F("</p></div>\n"));
}

void WebseiteStartAusgeben() {
  logger.debug(F("Beginn von WebsiteStartAusgeben()"));

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeaderRefresh);
  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent_P(PSTR(
    "<div class=\"tuerkis\">"
    "<p>Diese Seite zeigt die Sensordaten deines Pflanzensensors an. Sie aktualisiert sich automatisch jede Minute.</p>"
    "</div>\n"));
  #if !MODUL_WEBHOOK
    bool webhookAn = false; // ansonsten ist die Variable nicht definiert und das Programm kompiliert nicht
  #endif

  #if MODUL_HELLIGKEIT
    sendeSensorDaten(helligkeitName, helligkeitFarbe, helligkeitMesswertProzent, F("%"), helligkeitWebhook, webhookAn);
  #endif

  #if MODUL_BODENFEUCHTE
    sendeSensorDaten(F("Bodenfeuchte"), bodenfeuchteFarbe, bodenfeuchteMesswertProzent, F("%"), bodenfeuchteWebhook, webhookAn);
  #endif

  #if MODUL_DHT
    sendeSensorDaten(F("Lufttemperatur"), lufttemperaturFarbe, lufttemperaturMesswert, F("°C"), lufttemperaturWebhook, webhookAn);
    sendeSensorDaten(F("Luftfeuchte"), luftfeuchteFarbe, luftfeuchteMesswert, F("%"), luftfeuchteWebhook, webhookAn);
  #endif

  #if MODUL_ANALOG3
    sendeSensorDaten(analog3Name, analog3Farbe, analog3MesswertProzent, F("%"), analog3Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG4
    sendeSensorDaten(analog4Name, analog4Farbe, analog4MesswertProzent, F("%"), analog4Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG5
    sendeSensorDaten(analog5Name, analog5Farbe, analog5MesswertProzent, F("%"), analog5Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG6
    sendeSensorDaten(analog6Name, analog6Farbe, analog6MesswertProzent, F("%"), analog6Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG7
    sendeSensorDaten(analog7Name, analog7Farbe, analog7MesswertProzent, F("%"), analog7Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG8
    sendeSensorDaten(analog8Name, analog8Farbe, analog8MesswertProzent, F("%"), analog8Webhook, webhookAn);
  #endif

  Webserver.sendContent_P(PSTR(
    "<h2>Links</h2>\n"
    "<div class=\"tuerkis\">\n"
    "<ul>\n"
    "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"));

  Webserver.sendContent_P(PSTR("<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"));

  Webserver.sendContent_P(PSTR(
    "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
    "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
    "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
    "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
    "</ul>\n"
    "</div>\n"));

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();

  logger.debug(F("Ende von WebsiteStartAusgeben()"));
}
