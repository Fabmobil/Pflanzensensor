void sendeSensorDaten(const __FlashStringHelper* sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit) {
  Webserver.sendContent(F("<h2>"));
  Webserver.sendContent(sensorName);
  Webserver.sendContent(F("</h2>\n<div class=\""));
  Webserver.sendContent(sensorFarbe);
  Webserver.sendContent(F("\"><p>"));
  Webserver.sendContent(String(messwert));
  Webserver.sendContent(F(" "));
  Webserver.sendContent(einheit);
  Webserver.sendContent(F("</p></div>\n"));
}

void sendeAnalogsensorDaten(int sensorNummer, const String& sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit) {
  Webserver.sendContent(F("<h2>Analogsensor "));
  Webserver.sendContent(String(sensorNummer));
  Webserver.sendContent(F(": "));
  Webserver.sendContent(sensorName);
  Webserver.sendContent(F("</h2>\n<div class=\""));
  Webserver.sendContent(sensorFarbe);
  Webserver.sendContent(F("\"><p>"));
  Webserver.sendContent(String(messwert));
  Webserver.sendContent(F(" "));
  Webserver.sendContent(einheit);
  Webserver.sendContent(F("</p></div>\n"));
}

void WebseiteStartAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebsiteStartAusgeben()"));
  #endif

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent_P(PSTR(
    "<div class=\"weiss\">"
    "<p>Diese Seite zeigt die Sensordaten deines Pflanzensensors an. Sie aktualisiert sich automatisch aller 10 Sekunden.</p>"
    "</div>\n"));

  #if MODUL_HELLIGKEIT
    sendeSensorDaten(F("Helligkeit"), helligkeitFarbe, helligkeitMesswertProzent, F("%"));
  #endif

  #if MODUL_BODENFEUCHTE
    sendeSensorDaten(F("Bodenfeuchte"), bodenfeuchteFarbe, bodenfeuchteMesswertProzent, F("%"));
  #endif

  #if MODUL_DHT
    sendeSensorDaten(F("Lufttemperatur"), lufttemperaturFarbe, lufttemperaturMesswert, F("°C"));
    sendeSensorDaten(F("Luftfeuchte"), luftfeuchteFarbe, luftfeuchteMesswert, F("%"));
  #endif

  #if MODUL_ANALOG3
    sendeAnalogsensorDaten(3, analog3Name, analog3Farbe, analog3MesswertProzent, F("%"));
  #endif

  #if MODUL_ANALOG4
    sendeAnalogsensorDaten(4, analog4Name, analog4Farbe, analog4MesswertProzent, F("%"));
  #endif

  #if MODUL_ANALOG5
    sendeAnalogsensorDaten(5, analog5Name, analog5Farbe, analog5MesswertProzent, F("%"));
  #endif

  #if MODUL_ANALOG6
    sendeAnalogsensorDaten(6, analog6Name, analog6Farbe, analog6MesswertProzent, F("%"));
  #endif

  #if MODUL_ANALOG7
    sendeAnalogsensorDaten(7, analog7Name, analog7Farbe, analog7MesswertProzent, F("%"));
  #endif

  #if MODUL_ANALOG8
    sendeAnalogsensorDaten(8, analog8Name, analog8Farbe, analog8MesswertProzent, F("%"));
  #endif

  Webserver.sendContent_P(PSTR(
    "<h2>Links</h2>\n"
    "<div class=\"weiss\">\n"
    "<ul>\n"
    "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"));

  #if MODUL_DEBUG
    Webserver.sendContent_P(PSTR("<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"));
  #endif

  Webserver.sendContent_P(PSTR(
    "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
    "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
    "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
    "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
    "</ul>\n"
    "</div>\n"));

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();

  #if MODUL_DEBUG
    Serial.println(F("# Ende von WebsiteStartAusgeben()"));
  #endif
}
