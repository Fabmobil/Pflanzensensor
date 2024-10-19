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
  sendeEinstellung(F("aktueller absoluter Messwert"), String(prefix) + F("Minimum"), String(messwert));
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

  // Hier folgt der Rest des Codes für die WLAN-Konfigurationen...

  // Log Einstellungen
  Webserver.sendContent(F("<h2>Log Einstellungen</h2>\n<div class=\"tuerkis\">\n"));
  sendeEinstellung(F("Log Level"), F("logLevel"), logLevel);
  sendeEinstellung(F("Anzahl Einträge im Log"), F("logAnzahlEintraege"), String(logAnzahlEintraege));
  sendeEinstellung(F("Anzahl Einträge auf Webseite"), F("logAnzahlWebseite"), String(logAnzahlWebseite));
  sendeCheckbox(F("Log in Datei aktiviert?"), F("logInDatei"), logInDatei);
  Webserver.sendContent(F("</div>\n"));

  // Hier folgen die weiteren Moduleinstellungen...

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

  logger.debug("Ende von WebsiteAdminAusgeben()");
}
