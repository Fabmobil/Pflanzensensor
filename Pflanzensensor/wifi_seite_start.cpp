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

void sendeSensorDaten(const __FlashStringHelper* sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit, bool alarm, bool webhook) {
  char buffer[200];
  snprintf_P(buffer, sizeof(buffer), PSTR("<h2>%s%s</h2>\n<div class=\"%s\"><p>%d %s</p></div>\n"),
             reinterpret_cast<const char*>(sensorName),
             (alarm && webhook) ? " ⏰" : "",
             sensorFarbe.c_str(),
             messwert,
             reinterpret_cast<const char*>(einheit));
  Webserver.sendContent(buffer);
}

void sendeAnalogsensorDaten(int sensorNummer, const String& sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit, bool alarm, bool webhook) {
  char buffer[200];
  snprintf_P(buffer, sizeof(buffer), PSTR("<h2>Analogsensor %d: %s%s</h2>\n<div class=\"%s\"><p>%d %s</p></div>\n"),
             sensorNummer,
             sensorName.c_str(),
             (alarm && webhook) ? " ⏰" : "",
             sensorFarbe.c_str(),
             messwert,
             reinterpret_cast<const char*>(einheit));
  Webserver.sendContent(buffer);
}

void WebseiteStartAusgeben() {
  logger.debug(F("Beginn von WebsiteStartAusgeben()"));

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  sendeHtmlHeader(Webserver, true);

  static const char PROGMEM introText[] =
    "<div class=\"tuerkis\">"
    "<p>Diese Seite zeigt die Sensordaten deines Pflanzensensors an. Sie aktualisiert sich automatisch jede Minute.</p>"
    "</div>\n";
  Webserver.sendContent_P(introText);

  #if !MODUL_WEBHOOK
    bool webhookAn = false; // ansonsten ist die Variable nicht definiert und das Programm kompiliert nicht
  #endif

  #if MODUL_HELLIGKEIT
    sendeSensorDaten(F("Helligkeit"), helligkeitFarbe, helligkeitMesswertProzent, F("%"), helligkeitWebhook, webhookAn);
  #endif

  #if MODUL_BODENFEUCHTE
    sendeSensorDaten(F("Bodenfeuchte"), bodenfeuchteFarbe, bodenfeuchteMesswertProzent, F("%"), bodenfeuchteWebhook, webhookAn);
  #endif

  #if MODUL_DHT
    sendeSensorDaten(F("Lufttemperatur"), lufttemperaturFarbe, lufttemperaturMesswert, F("°C"), lufttemperaturWebhook, webhookAn);
    sendeSensorDaten(F("Luftfeuchte"), luftfeuchteFarbe, luftfeuchteMesswert, F("%"), luftfeuchteWebhook, webhookAn);
  #endif

  #if MODUL_ANALOG3
    sendeAnalogsensorDaten(3, analog3Name, analog3Farbe, analog3MesswertProzent, F("%"), analog3Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG4
    sendeAnalogsensorDaten(4, analog4Name, analog4Farbe, analog4MesswertProzent, F("%"), analog4Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG5
    sendeAnalogsensorDaten(5, analog5Name, analog5Farbe, analog5MesswertProzent, F("%"), analog5Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG6
    sendeAnalogsensorDaten(6, analog6Name, analog6Farbe, analog6MesswertProzent, F("%"), analog6Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG7
    sendeAnalogsensorDaten(7, analog7Name, analog7Farbe, analog7MesswertProzent, F("%"), analog7Webhook, webhookAn);
  #endif

  #if MODUL_ANALOG8
    sendeAnalogsensorDaten(8, analog8Name, analog8Farbe, analog8MesswertProzent, F("%"), analog8Webhook, webhookAn);
  #endif

  static const char PROGMEM linksSection[] =
    "<h2>Links</h2>\n"
    "<div class=\"tuerkis\">\n"
    "<ul>\n"
    "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"
    "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"
    "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
    "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
    "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
    "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
    "</ul>\n"
    "</div>\n";
  Webserver.sendContent_P(linksSection);

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();

  logger.debug(F("Ende von WebsiteStartAusgeben()"));
}
