/**
 * @file wifi_seite_admin.h
 * @brief Administrationsseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält Funktionen zur Generierung und Verarbeitung
 * der Administrationsseite des Pflanzensensors.
 */

#ifndef WIFI_SEITE_ADMIN_H
#define WIFI_SEITE_ADMIN_H

/**
 * @brief Sendet eine Einstellungsoption an den Webserver
 *
 * @param bezeichnung Die Bezeichnung der Einstellung
 * @param name Der Name des Eingabefeldes
 * @param wert Der aktuelle Wert der Einstellung
 */
void sendeEinstellung(const __FlashStringHelper* bezeichnung, const String& name, const String& wert) {
  Webserver.sendContent(F("<p>"));
  Webserver.sendContent(bezeichnung);
  Webserver.sendContent(F(": <input type=\"text\" size=\"20\" name=\""));
  Webserver.sendContent(name);
  Webserver.sendContent(F("\" placeholder=\""));
  Webserver.sendContent(wert);
  Webserver.sendContent(F("\"></p>\n"));
}

/**
 * @brief Sendet eine Checkbox-Option an den Webserver
 *
 * @param bezeichnung Die Bezeichnung der Checkbox
 * @param name Der Name der Checkbox
 * @param status Der aktuelle Status der Checkbox (true/false)
 */
void sendeCheckbox(const __FlashStringHelper* bezeichnung, const String& name, const bool& status) {
  Webserver.sendContent(F("<p>"));
  Webserver.sendContent(bezeichnung);
  Webserver.sendContent(F(" <input type=\"checkbox\" name=\""));
  Webserver.sendContent(name);
  Webserver.sendContent(F("\""));
  if (status == 1) { Webserver.sendContent(F(" checked")); }
  Webserver.sendContent(F("></p>\n"));
}

/**
 * @brief Sendet Schwellwert-Einstellungen an den Webserver
 *
 * @param prefix Das Präfix für die Eingabefeld-Namen
 * @param gruenUnten Unterer grüner Schwellwert
 * @param gruenOben Oberer grüner Schwellwert
 * @param gelbUnten Unterer gelber Schwellwert
 * @param gelbOben Oberer gelber Schwellwert
 */
void sendeSchwellwerte(const __FlashStringHelper* prefix, int gruenUnten, int gruenOben, int gelbUnten, int gelbOben) {
  sendeEinstellung(F("unterer gelber Schwellwert"), String(prefix) + F("GelbUnten"), String(gelbUnten));
  sendeEinstellung(F("unterer grüner Schwellwert"), String(prefix) + F("GruenUnten"), String(gruenUnten));
  sendeEinstellung(F("oberer grüner Schwellwert"), String(prefix) + F("GruenOben"), String(gruenOben));
  sendeEinstellung(F("oberer gelber Schwellwert"), String(prefix) + F("GelbOben"), String(gelbOben));
}

/**
 * @brief Sendet die Einstellungen für einen Analogsensor an den Webserver
 *
 * @param titel Der Titel des Sensorabschnitts
 * @param prefix Das Präfix für die Eingabefeld-Namen
 * @param sensorName Der Name des Sensors
 * @param minimum Der Minimalwert des Sensors
 * @param maximum Der Maximalwert des Sensors
 * @param gruenUnten Unterer grüner Schwellwert
 * @param gruenOben Oberer grüner Schwellwert
 * @param gelbUnten Unterer gelber Schwellwert
 * @param gelbOben Oberer gelber Schwellwert
 * @param alarm Status des Alarms für diesen Sensor
 * @param messwert Aktueller Messwert des Sensors
 */
void sendeAnalogsensorEinstellungen(const __FlashStringHelper* titel, const __FlashStringHelper* prefix, const String& sensorName, int minimum, int maximum,
                                    int gruenUnten, int gruenOben, int gelbUnten, int gelbOben, bool alarm, int messwert) {
  Webserver.sendContent(F("<h2>"));
  Webserver.sendContent(titel);
  Webserver.sendContent(F("</h2>\n<div class=\"tuerkis\">\n"));
  sendeCheckbox(F("Alarm aktiv?"), String(prefix) + F("Webhook"), alarm);
  sendeEinstellung(F("Sensorname"), String(prefix) + F("Name"), sensorName);
  sendeEinstellung(F("aktueller absoluter Messwert"), String(prefix) + F("Minimum"), String(messwert));
  sendeEinstellung(F("Minimalwert (trocken/dunkel)"), String(prefix) + F("Minimum"), String(minimum));
  sendeEinstellung(F("Maximalwert (feucht/hell)"), String(prefix) + F("Maximum"), String(maximum));

  sendeSchwellwerte(prefix, gruenUnten, gruenOben, gelbUnten, gelbOben);

  Webserver.sendContent(F("</div>\n"));
}

/**
 * @brief Sendet Links an den Webserver
 */
void sendeLinks() {
  Webserver.sendContent_P(PSTR(
    "<h2>Links</h2>\n"
    "<div class=\"tuerkis\">\n"
    "<ul>\n"
    "<li><a href=\"/\">zur Startseite</a></li>\n"));

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
}

/**
 * @brief Generiert und sendet die Administrationsseite
 */
void WebseiteAdminAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebsiteAdminAusgeben()"));
  #endif

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeaderNoRefresh);
  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent_P(PSTR(
    "<div class=\"tuerkis\"><p>Auf dieser Seite können die Variablen verändert werden.</p>\n"
    "<p>Die Felder zeigen in grau die derzeit gesetzten Werte an. Falls kein neuer Wert eingegeben wird, bleibt der alte Wert erhalten.</p>\n</div>\n"
    "<form action=\"/setzeVariablen\" method=\"POST\">\n"));

    // Neuer Abschnitt für WIFI-Einstellungen
  Webserver.sendContent(F("<h2>WIFI-Einstellungen</h2>\n<div class=\"tuerkis\">\n"));

  if (wifiAp) {
    Webserver.sendContent(F("<p>Gerät befindet sich im Accesspoint-Modus. Alle WLAN-Einstellungen sind editierbar.</p>\n"));

    // Checkbox zum Wechsel in den normalen WLAN-Modus
    sendeCheckbox(F("In normalen WLAN-Modus wechseln"), F("wechselZuWLAN"), false);

    // WLAN 1
    Webserver.sendContent(F("<h3>WLAN 1</h3>\n"));
    sendeEinstellung(F("SSID"), F("wifiSsid1"), wifiSsid1);
    sendeEinstellung(F("Passwort"), F("wifiPassword1"), F("********"));

    // WLAN 2
    Webserver.sendContent(F("<h3>WLAN 2</h3>\n"));
    sendeEinstellung(F("SSID"), F("wifiSsid2"), wifiSsid2);
    sendeEinstellung(F("Passwort"), F("wifiPassword2"), F("********"));

    // WLAN 3
    Webserver.sendContent(F("<h3>WLAN 3</h3>\n"));
    sendeEinstellung(F("SSID"), F("wifiSsid3"), wifiSsid3);
    sendeEinstellung(F("Passwort"), F("wifiPassword3"), F("********"));
  } else {
    // Aktuelle Verbindung ermitteln
    String currentSSID = WiFi.SSID();

    // WLAN 1
    Webserver.sendContent(F("<h3>WLAN 1</h3>\n"));
    if (currentSSID == wifiSsid1) {
      Webserver.sendContent(F("<p>SSID: "));
      Webserver.sendContent(wifiSsid1);
      Webserver.sendContent(F(" (aktuelle Verbindung, nicht editierbar)</p>\n"));
    } else {
      sendeEinstellung(F("SSID"), F("wifiSsid1"), wifiSsid1);
      sendeEinstellung(F("Passwort"), F("wifiPassword1"), F("********"));
    }

    // WLAN 2
    Webserver.sendContent(F("<h3>WLAN 2</h3>\n"));
    if (currentSSID == wifiSsid2) {
      Webserver.sendContent(F("<p>SSID: "));
      Webserver.sendContent(wifiSsid2);
      Webserver.sendContent(F(" (aktuelle Verbindung, nicht editierbar)</p>\n"));
    } else {
      sendeEinstellung(F("SSID"), F("wifiSsid2"), wifiSsid2);
      sendeEinstellung(F("Passwort"), F("wifiPassword2"), F("********"));
    }

    // WLAN 3
    Webserver.sendContent(F("<h3>WLAN 3</h3>\n"));
    if (currentSSID == wifiSsid3) {
      Webserver.sendContent(F("<p>SSID: "));
      Webserver.sendContent(wifiSsid3);
      Webserver.sendContent(F(" (aktuelle Verbindung, nicht editierbar)</p>\n"));
    } else {
      sendeEinstellung(F("SSID"), F("wifiSsid3"), wifiSsid3);
      sendeEinstellung(F("Passwort"), F("wifiPassword3"), F("********"));
    }
  }

  Webserver.sendContent(F("</div>\n"));

    #if MODUL_WEBHOOK
      Webserver.sendContent_P(PSTR("<h2>Webhook Modul</h2>\n<div class=\"tuerkis\">\n"));
      sendeCheckbox(F("Webhook aktiv?"), F("webhookAn"), webhookAn);
      sendeEinstellung(F("Alarm-Benachrichtigungsfrequenz in Stunden"), F("webhookFrequenz"), String(webhookFrequenz));
      sendeEinstellung(F("Ping-Benachrichtigungsfrequenz in Stunden"), F("webhookPingFrequenz"), String(webhookPingFrequenz));
      sendeEinstellung(F("Domain des Webhooks"), F("webhookDomain"), webhookDomain);
      sendeEinstellung(F("Schlüssel/Pfad des Webhooks"), F("webhookPfad"), webhookPfad);
      Webserver.sendContent(F("</div>\n"));
    #endif

    #if MODUL_LEDAMPEL
      Webserver.sendContent_P(PSTR("<h2>LED Ampel</h2>\n<div class=\"tuerkis\">\n"));
      sendeCheckbox(F("LED Ampel angeschalten?"), F("ampelAn"), ampelAn);
      sendeEinstellung(F("Modus: (0: Anzeige der Bodenfeuchte; 1: Anzeige aller Sensoren hintereinander analog zu dem, was auf dem Display steht)"), F("ampelModus"), String(ampelModus));
      Webserver.sendContent(F("</div>\n"));
    #endif

    #if MODUL_DISPLAY
      Webserver.sendContent_P(PSTR("<h2>Display</h2><div class=\"tuerkis\">\n"));
      sendeCheckbox(F("Display angeschalten?"), F("displayAn"), displayAn);
      Webserver.sendContent(F("</div>\n"));
    #endif

    #if MODUL_BODENFEUCHTE
      sendeAnalogsensorEinstellungen(F("Bodenfeuchte"), F("bodenfeuchte"), bodenfeuchteName, bodenfeuchteMinimum, bodenfeuchteMaximum,
                              bodenfeuchteGruenUnten, bodenfeuchteGruenOben, bodenfeuchteGelbUnten, bodenfeuchteGelbOben, bodenfeuchteWebhook, bodenfeuchteMesswert);
    #endif

    #if MODUL_DHT
      Webserver.sendContent_P(PSTR("<h2>DHT Modul</h2>\n<h3>Lufttemperatur</h3>\n<div class=\"tuerkis\">\n"));
      sendeCheckbox(F("Alarm aktiv?"), F("lufttemperaturWebhook"), lufttemperaturWebhook);
      sendeSchwellwerte(F("lufttemperatur"), lufttemperaturGruenUnten, lufttemperaturGruenOben, lufttemperaturGelbUnten, lufttemperaturGelbOben);
      Webserver.sendContent(F("</div>\n"));
      Webserver.sendContent_P(PSTR("<h3>Luftfeuchte</h3>\n<div class=\"tuerkis\">\n"));
      sendeCheckbox(F("Alarm aktiv?"), F("luftfeuchteWebhook"), luftfeuchteWebhook);
      sendeSchwellwerte(F("luftfeuchte"), luftfeuchteGruenUnten, luftfeuchteGruenOben, luftfeuchteGelbUnten, luftfeuchteGelbOben);
      Webserver.sendContent(F("</div>\n"));
    #endif


    #if MODUL_HELLIGKEIT
      sendeAnalogsensorEinstellungen(F("Helligkeitssensor"), F("helligkeit"), helligkeitName, helligkeitMinimum, helligkeitMaximum,
                              helligkeitGruenUnten, helligkeitGruenOben, helligkeitGelbUnten, helligkeitGelbOben, helligkeitWebhook, helligkeitMesswert);
    #endif

    // Analogsensoren
    #if MODUL_ANALOG3
      sendeAnalogsensorEinstellungen(F("Analogsensor 3"), F("analog3"), analog3Name, analog3Minimum, analog3Maximum, analog3GruenUnten, analog3GruenOben, analog3GelbUnten, analog3GelbOben, analog3Webhook, analog3Messwert);
    #endif
    #if MODUL_ANALOG4
      sendeAnalogsensorEinstellungen(F("Analogsensor 4"), F("analog4"), analog4Name, analog4Minimum, analog4Maximum, analog4GruenUnten, analog4GruenOben, analog4GelbUnten, analog4GelbOben, analog4Webhook, analog4Messwert);
    #endif
    #if MODUL_ANALOG5
      sendeAnalogsensorEinstellungen(F("Analogsensor 5"), F("analog5"), analog5Name, analog5Minimum, analog5Maximum, analog5GruenUnten, analog5GruenOben, analog5GelbUnten, analog5GelbOben, analog5Webhook, analog4Messwert);
    #endif
    #if MODUL_ANALOG6
      sendeAnalogsensorEinstellungen(F("Analogsensor 6"), F("analog6"), analog6Name, analog6Minimum, analog6Maximum, analog6GruenUnten, analog6GruenOben, analog6GelbUnten, analog6GelbOben, analog6Webhook, analog4Messwert);
    #endif
    #if MODUL_ANALOG7
      sendeAnalogsensorEinstellungen(F("Analogsensor 7"), F("analog7"), analog7Name, analog7Minimum, analog7Maximum, analog7GruenUnten, analog7GruenOben, analog7GelbUnten, analog7GelbOben, analog7Webhook, analog4Messwert);
    #endif
    #if MODUL_ANALOG8
      sendeAnalogsensorEinstellungen(F("Analogsensor 8"), F("analog8"), analog8Name, analog8Minimum, analog8Maximum, analog8GruenUnten, analog8GruenOben, analog8GelbUnten, analog8GelbOben, analog8Webhook, analog4Messwert);
    #endif

    Webserver.sendContent_P(PSTR(
      "<h2>Einstellungen löschen?</h2>\n"
      "<div class=\"rot\">\n<p>"
      "GEFAHR: Wenn du hier \"Ja!\" eingibst, werden alle Einstellungen gelöscht und die Werte, "
      "die beim Flashen eingetragen wurden, werden wieder gesetzt. Der Pflanzensensor startet neu."
      "</p>\n<p><input type=\"text\" size=\"4\" name=\"loeschen\" placeholder=\"nein\"></p>\n</div>\n"
      "<h2>Passwort</h2>\n"
      "<div class=\"tuerkis\">"
      "<p><input type=\"password\" name=\"Passwort\" placeholder=\"Passwort\"><br>"
      "<input type=\"submit\" value=\"Absenden\"></p></form>"
      "</div>\n"));

    sendeLinks();

    Webserver.sendContent_P(htmlFooter);
    Webserver.client().flush();

    #if MODUL_DEBUG
      Serial.println(F("# Ende von WebsiteAdminAusgeben()"));
    #endif
}

#endif // WIFI_SEITE_ADMIN_H

