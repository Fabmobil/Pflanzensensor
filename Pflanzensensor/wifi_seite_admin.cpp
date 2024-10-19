/**
 * @file wifi_seite_admin.cpp
 * @brief Implementierung der Administrationsseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Implementierungen der Funktionen zur Generierung und Verarbeitung
 * der Administrationsseite des Pflanzensensors.
 */

#include "wifi_seite_admin.h"
#include "einstellungen.h"
#include "logger.h"
#include "wifi_header.h"
#include "wifi_footer.h"

extern ESP8266WebServer Webserver;
extern bool wifiAp;

void sendeEinstellung(const __FlashStringHelper* bezeichnung, const String& name, const String& wert) {
  Webserver.sendContent(F("<p>"));
  Webserver.sendContent(bezeichnung);
  Webserver.sendContent(F(": <input type=\"text\" size=\"20\" name=\""));
  Webserver.sendContent(name);
  Webserver.sendContent(F("\" placeholder=\""));
  Webserver.sendContent(wert);
  Webserver.sendContent(F("\"></p>\n"));
}

void sendeCheckbox(const __FlashStringHelper* bezeichnung, const String& name, const bool& status) {
  Webserver.sendContent(F("<p>"));
  Webserver.sendContent(bezeichnung);
  Webserver.sendContent(F(" <input type=\"checkbox\" name=\""));
  Webserver.sendContent(name);
  Webserver.sendContent(F("\""));
  if (status == 1) { Webserver.sendContent(F(" checked")); }
  Webserver.sendContent(F("></p>\n"));
}

void sendeSchwellwerte(const __FlashStringHelper* prefix, int gruenUnten, int gruenOben, int gelbUnten, int gelbOben) {
  sendeEinstellung(F("unterer gelber Schwellwert"), String(prefix) + F("GelbUnten"), String(gelbUnten));
  sendeEinstellung(F("unterer grüner Schwellwert"), String(prefix) + F("GruenUnten"), String(gruenUnten));
  sendeEinstellung(F("oberer grüner Schwellwert"), String(prefix) + F("GruenOben"), String(gruenOben));
  sendeEinstellung(F("oberer gelber Schwellwert"), String(prefix) + F("GelbOben"), String(gelbOben));
}

void sendeAnalogsensorEinstellungen(const __FlashStringHelper* titel, const __FlashStringHelper* prefix, const String& sensorName, int minimum, int maximum,
                                    int gruenUnten, int gruenOben, int gelbUnten, int gelbOben, bool alarm, int messwert) {
  Webserver.sendContent(F("<h2>"));
  Webserver.sendContent(titel);
  Webserver.sendContent(F("</h2>\n<div class=\"tuerkis\">\n"));
  #if MODUL_WEBHOOK
    sendeCheckbox(F("Alarm aktiv?"), String(prefix) + F("Webhook"), alarm);
  #endif
  sendeEinstellung(F("Sensorname"), String(prefix) + F("Name"), sensorName);
  Webserver.sendContent(F("<p>Aktueller absoluter Messwert: <span id=\""));
  Webserver.sendContent(String(prefix));
  Webserver.sendContent(F("Messwert\">"));
  Webserver.sendContent(String(messwert));
  Webserver.sendContent(F("</span></p>\n"));
  sendeEinstellung(F("Minimalwert (trocken/dunkel)"), String(prefix) + F("Minimum"), String(minimum));
  sendeEinstellung(F("Maximalwert (feucht/hell)"), String(prefix) + F("Maximum"), String(maximum));

  sendeSchwellwerte(prefix, gruenUnten, gruenOben, gelbUnten, gelbOben);

  Webserver.sendContent(F("</div>\n"));
}

void sendeLinks() {
  Webserver.sendContent_P(PSTR(
    "<h2>Links</h2>\n"
    "<div class=\"tuerkis\">\n"
    "<ul>\n"
    "<li><a href=\"/\">zur Startseite</a></li>\n"
    "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"
    "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
    "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
    "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
    "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
    "</ul>\n"
    "</div>\n"));
}

void WebseiteAdminAusgeben() {
  logger.debug("Beginn von WebsiteAdminAusgeben()");

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeaderNoRefresh);
  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent_P(PSTR(
    "<div class=\"tuerkis\"><p>Auf dieser Seite können die Variablen verändert werden.</p>\n"
    "<p>Die Felder zeigen in grau die derzeit gesetzten Werte an. Falls kein neuer Wert eingegeben wird, bleibt der alte Wert erhalten.</p>\n</div>\n"
    "<form action=\"/setzeVariablen\" method=\"POST\">\n"));

  // WIFI-Einstellungen
  Webserver.sendContent(F("<h2>WIFI-Einstellungen</h2>\n<div class=\"tuerkis\">\n"));
  Webserver.sendContent(F("<p>Modus:<br>"));
  Webserver.sendContent(F("<input type=\"radio\" name=\"wlanModus\" value=\"ap\""));
  if (wifiAp) { Webserver.sendContent(F(" checked")); }
  Webserver.sendContent(F("> Access Point<br>"));
  Webserver.sendContent(F("<input type=\"radio\" name=\"wlanModus\" value=\"wlan\""));
  if (!wifiAp) { Webserver.sendContent(F(" checked")); }
  Webserver.sendContent(F("> WLAN Client"));
  Webserver.sendContent(F("</p>\n</div>\n"));
    Webserver.sendContent(F("<h3>WLAN Konfigurationen</h3>\n<div class=\"tuerkis\">\n"));
    if (wifiAp) {
        Webserver.sendContent(F("<p>Gerät befindet sich im Accesspoint-Modus. Alle WLAN-Einstellungen sind editierbar.</p>\n"));
        Webserver.sendContent(F("</div>\n"));
        // WLAN 1
        Webserver.sendContent(F("<h4>WLAN 1</h4>\n<div class=\"tuerkis\">\n"));
        sendeEinstellung(F("SSID"), F("wifiSsid1"), wifiSsid1);
        sendeEinstellung(F("Passwort"), F("wifiPassword1"), F("********"));
        Webserver.sendContent(F("</div>\n"));

        // WLAN 2
        Webserver.sendContent(F("<h4>WLAN 2</h4>\n<div class=\"tuerkis\">\n"));
        sendeEinstellung(F("SSID"), F("wifiSsid2"), wifiSsid2);
        sendeEinstellung(F("Passwort"), F("wifiPassword2"), F("********"));
        Webserver.sendContent(F("</div>\n"));

        // WLAN 3
        Webserver.sendContent(F("<h4>WLAN 3</h4>\n<div class=\"tuerkis\">\n"));
        sendeEinstellung(F("SSID"), F("wifiSsid3"), wifiSsid3);
        sendeEinstellung(F("Passwort"), F("wifiPassword3"), F("********"));
        Webserver.sendContent(F("</div>\n"));
    } else {
        Webserver.sendContent(F("</div>\n"));
        // Aktuelle Verbindung ermitteln
        String currentSSID = WiFi.SSID();

        // WLAN 1
        Webserver.sendContent(F("<h4>WLAN 1</h4>\n<div class=\"tuerkis\">\n"));
        if (currentSSID == wifiSsid1) {
        Webserver.sendContent(F("<p>SSID: "));
        Webserver.sendContent(wifiSsid1);
        Webserver.sendContent(F(" (aktive Verbindung ist nicht editierbar)</p>\n"));
        } else {
        sendeEinstellung(F("SSID"), F("wifiSsid1"), wifiSsid1);
        sendeEinstellung(F("Passwort"), F("wifiPassword1"), F("********"));
        }
        Webserver.sendContent(F("</div>\n"));

        // WLAN 2
        Webserver.sendContent(F("<h4>WLAN 2</h4>\n<div class=\"tuerkis\">\n"));
        if (currentSSID == wifiSsid2) {
        Webserver.sendContent(F("<p>SSID: "));
        Webserver.sendContent(wifiSsid2);
        Webserver.sendContent(F(" (aktuelle Verbindung, nicht editierbar)</p>\n"));
        } else {
        sendeEinstellung(F("SSID"), F("wifiSsid2"), wifiSsid2);
        sendeEinstellung(F("Passwort"), F("wifiPassword2"), F("********"));
        }
        Webserver.sendContent(F("</div>\n"));

        // WLAN 3
        Webserver.sendContent(F("<h4>WLAN 3</h4>\n<div class=\"tuerkis\">\n"));
        if (currentSSID == wifiSsid3) {
        Webserver.sendContent(F("<p>SSID: "));
        Webserver.sendContent(wifiSsid3);
        Webserver.sendContent(F(" (aktuelle Verbindung, nicht editierbar)</p>\n"));
        } else {
        sendeEinstellung(F("SSID"), F("wifiSsid3"), wifiSsid3);
        sendeEinstellung(F("Passwort"), F("wifiPassword3"), F("********"));
        }
        Webserver.sendContent(F("</div>\n"));
    }
    Webserver.sendContent(F("<h3>Access Point Einstellungen</h3>\n<div class=\"tuerkis\">\n"));
    sendeEinstellung(F("AP SSID"), F("wifiApSsid"), wifiApSsid);
    sendeCheckbox(F("AP Passwort aktivieren"), F("wifiApPasswortAktiviert"), wifiApPasswortAktiviert);
    sendeEinstellung(F("AP Passwort"), F("wifiApPasswort"), wifiApPasswortAktiviert ? wifiApPasswort : F("********"));
    Webserver.sendContent(F("</div>\n"));

  // Log Einstellungen
  Webserver.sendContent(F("<h2>Log Einstellungen</h2>\n<div class=\"tuerkis\">\n"));
  sendeEinstellung(F("Log Level"), F("logLevel"), logLevel);
  sendeEinstellung(F("Anzahl Einträge im Log"), F("logAnzahlEintraege"), String(logAnzahlEintraege));
  sendeEinstellung(F("Anzahl Einträge auf Webseite"), F("logAnzahlWebseite"), String(logAnzahlWebseite));
  sendeCheckbox(F("Log in Datei aktiviert?"), F("logInDatei"), logInDatei);
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
      #if MODUL_WEBHOOK
        sendeCheckbox(F("Alarm aktiv?"), F("lufttemperaturWebhook"), lufttemperaturWebhook);
      #endif
      sendeSchwellwerte(F("lufttemperatur"), lufttemperaturGruenUnten, lufttemperaturGruenOben, lufttemperaturGelbUnten, lufttemperaturGelbOben);
      Webserver.sendContent(F("</div>\n"));
      Webserver.sendContent_P(PSTR("<h3>Luftfeuchte</h3>\n<div class=\"tuerkis\">\n"));
      #if MODUL_WEBHOOK
        sendeCheckbox(F("Alarm aktiv?"), F("luftfeuchteWebhook"), luftfeuchteWebhook);
      #endif
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
  Webserver.sendContent(F(R"=====(
<script>
function updateMeasurements() {
var xhttp = new XMLHttpRequest();
xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
    var measurements = JSON.parse(this.responseText);
    for (var key in measurements) {
        if (measurements.hasOwnProperty(key)) {
        document.getElementById(key + "Messwert").innerHTML = measurements[key];
        }
    }
    }
};
xhttp.open("GET", "/leseMesswerte", true);
xhttp.send();
}

setInterval(updateMeasurements, 5000); // Aktualisiere alle 5 Sekunden
</script>
)====="));
  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();

  logger.debug("Ende von WebsiteAdminAusgeben()");
}
