/**
 * @file wifi_seite_setzeVariablen.cpp
 * @brief Implementierung der Variablenverarbeitung für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Implementierungen der Funktionen zur Verarbeitung und Aktualisierung
 * von Variablen, die über die Weboberfläche geändert werden.
 */

#include "wifi_seite_setzeVariablen.h"
#include "einstellungen.h"
#include "passwoerter.h"
#include "variablenspeicher.h"
#include "wifi.h"
#include "wifi_footer.h"
#include "wifi_header.h"
#include "logger.h"

#if MODUL_DISPLAY
    extern int status;
#endif

void ArgumenteAusgeben() {
  logger.info(F("Gebe alle Argumente des POST requests aus:"));
  int numArgs = Webserver.args();
  for (int i = 0; i < numArgs; i++) {
    logger.info(Webserver.argName(i) + F(": ") + Webserver.arg(i));
  }
}

void WebseiteSetzeVariablen() {
    logger.debug(F("Beginn von WebseiteSetzeVariablen()"));

    millisVorherWebhook = millisAktuell;

    if (Webserver.arg(F("Passwort")) == wifiAdminPasswort) {
        // Zuerst die Variablen aktualisieren
        AktualisiereVariablen();

        // Wenn WLAN-Änderungen vorgenommen wurden, besondere Behandlung
        if (wlanAenderungVorgenommen) {
            // Sofort speichern
            VariablenSpeichern();

            // Genügend Zeit zum Speichern geben
            delay(1000);

            // HTTP Antwort senden
            Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
            Webserver.send(200, F("text/html"), "");
            sendeHtmlHeader(Webserver, false);

            static const char PROGMEM wlanAenderung[] =
                "<h3>WLAN-Einstellungen geändert</h3>\n"
                "<div class=\"rot\">\n"
                "<p>Die WLAN-Einstellungen wurden gespeichert. "
                "Der Pflanzensensor wird jetzt neu gestartet.</p>\n"
                "<p>Bitte warte einen Moment...</p>\n"
                "</div>\n";
            Webserver.sendContent_P(wlanAenderung);
            Webserver.sendContent_P(htmlFooter);

            // Sicherstellen, dass die Antwort gesendet wurde
            Webserver.client().flush();
            delay(1000);

            // Nochmal explizit speichern um sicher zu gehen
            VariablenSpeichern();

            // Längere Verzögerung vor dem Neustart
            delay(2000);

            // Erst dann neustarten
            ESP.restart();
            return;
        }

        // Jetzt erst die HTTP Antwort senden
        Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
        Webserver.send(200, F("text/html"), "");
        sendeHtmlHeader(Webserver, false);

        // Änderungen sammeln für die Anzeige
        static const char PROGMEM aenderungenStart[] = "<ul>\n";
        String aenderungen = FPSTR(aenderungenStart);

        // Speichern der alten Checkbox-Zustände
        bool alteCheckboxZustaende[8] = {
          #if MODUL_BODENFEUCHTE
              bodenfeuchteWebhook,
          #else
              false,
          #endif
          #if MODUL_HELLIGKEIT
              helligkeitWebhook,
          #else
              false,
          #endif
          #if MODUL_DHT
              lufttemperaturWebhook,
              luftfeuchteWebhook,
          #else
              false, false,
          #endif
          #if MODUL_LEDAMPEL
              ampelAn,
          #else
              false,
          #endif
          #if MODUL_DISPLAY
              displayAn,
          #else
              false,
          #endif
          #if MODUL_WEBHOOK
              webhookAn,
          #else
              false,
          #endif
          logInDatei
        };

        // Wir überprüfen jedes Eingabefeld und fügen Änderungen hinzu
        for (int i = 0; i < Webserver.args(); i++) {
            String argName = Webserver.argName(i);
            String argValue = Webserver.arg(i);

            if (argName != F("Passwort")) {
                // Spezieller Fall für den WLAN-Modus
                if (argName == F("wlanModus")) {
                    bool neuerWlanAp = (argValue == F("ap"));
                    if (neuerWlanAp != wifiAp) {
                        wlanAenderungVorgenommen = true;
                        char buffer[50];
                        snprintf_P(buffer, sizeof(buffer), PSTR("<li>WLAN-Modus: %s</li>\n"), neuerWlanAp ? "Access Point" : "WLAN Client");
                        aenderungen += buffer;
                    }
                }
                // Bei Checkboxen vergleichen wir mit dem alten Zustand
                else if (argName.endsWith(F("Webhook")) || argName == F("ampelAn") || argName == F("displayAn") || argName == F("webhookAn")) {
                    int index = -1;
                    if (argName == F("bodenfeuchteWebhook")) index = 0;
                    else if (argName == F("helligkeitWebhook")) index = 1;
                    else if (argName == F("lufttemperaturWebhook")) index = 2;
                    else if (argName == F("luftfeuchteWebhook")) index = 3;
                    else if (argName == F("ampelAn")) index = 4;
                    else if (argName == F("displayAn")) index = 5;
                    else if (argName == F("webhookAn")) index = 6;
                    else if (argName == F("logLevel")) index = 7;

                    if (index != -1) {
                        bool neuerZustand = Webserver.hasArg(argName);
                        if (neuerZustand != alteCheckboxZustaende[index]) {
                            char buffer[100];
                            snprintf_P(buffer, sizeof(buffer), PSTR("<li>%s: %s</li>\n"), argName.c_str(), neuerZustand ? "aktiviert" : "deaktiviert");
                            aenderungen += buffer;
                        }
                    }
                }
                // Für alle anderen Felder
                else if (argValue != F("")) {
                    char buffer[100];
                    snprintf_P(buffer, sizeof(buffer), PSTR("<li>%s: %s</li>\n"), argName.c_str(), argValue.c_str());
                    aenderungen += buffer;
                }
            }
        }

        // Überprüfen, ob Checkboxen deaktiviert wurden
        const char* checkboxNamen[] = {"bodenfeuchteWebhook", "helligkeitWebhook", "lufttemperaturWebhook", "luftfeuchteWebhook", "ampelAn", "displayAn"};
        for (int i = 0; i < 6; i++) {
            if (alteCheckboxZustaende[i] && !Webserver.hasArg(checkboxNamen[i])) {
                char buffer[100];
                snprintf_P(buffer, sizeof(buffer), PSTR("<li>%s: deaktiviert</li>\n"), checkboxNamen[i]);
                aenderungen += buffer;
            }
        }

        aenderungen += F("</ul>\n");

        Webserver.sendContent(F("<h3>Erfolgreich!</h3>\n"));
        Webserver.sendContent(F("<div class=\"gruen\">\n"));

        if (aenderungen == FPSTR(aenderungenStart)) {
            Webserver.sendContent(F("<p>Es wurden keine Änderungen vorgenommen.</p>\n"));
            logger.info(F("Es wurden keine Änderungen vorgenommen."));
        } else {
            Webserver.sendContent(F("<p>Folgende Änderungen wurden vorgenommen:</p>\n"));
            Webserver.sendContent(aenderungen);
            logger.info(F("Folgende Änderungen wurden vorgenommen:"));
            logger.info(aenderungen);
        }

        Webserver.sendContent(F("</div>"));

        static const char PROGMEM links[] =
            "<h3>Links</h3>\n"
            "<div class=\"tuerkis\">\n"
            "<ul>\n"
            "<li><a href=\"/\">zur Startseite</a></li>\n"
            "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"
            "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"
            "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
            "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
            "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
            "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
            "</ul>\n"
            "</div>\n";
        Webserver.sendContent_P(links);
        Webserver.sendContent_P(htmlFooter);
        Webserver.client().flush();

    } else {
        static const char PROGMEM falschesPasswort[] =
            "<h3>Falsches Passwort!</h3>\n<div class=\"rot\">\n"
            "<p>Du hast nicht das richtige Passwort eingebeben!</p></div>\n";
        Webserver.sendContent_P(falschesPasswort);
    }

    if (Webserver.arg(F("loeschen")) == F("Ja!")) {
        static const char PROGMEM loeschBestaetigung[] =
            "<div class=\"rot\">\n"
            "<p>Alle Variablen wurden gelöscht.</p>\n"
            "<p>Der Pflanzensensor wird neu gestartet.</p>\n"
            "</div>\n"
            "<div class=\"tuerkis\">\n"
            "<p><a href=\"/\">Warte ein paar Sekunden, dann kannst du hier zur Startseite zurück.</a></p>\n"
            "</div>\n";
        Webserver.sendContent_P(loeschBestaetigung);
        Webserver.sendContent_P(htmlFooter);
        Webserver.client().flush();
        VariablenLoeschen();
        delay(5);
        ESP.restart();
    }
}

void AktualisiereVariablen() {
  // WLAN Modus Änderung zuerst und sicherer behandeln
  wlanAenderungVorgenommen = false;
  if(Webserver.hasArg(F("wlanModus"))) {
    String neuerWLANModus = Webserver.arg(F("wlanModus"));
    // "ap" bedeutet AP Modus (wifiAp = true)
    // "wlan" bedeutet WLAN Modus (wifiAp = false)
    bool neuerWifiAp = (neuerWLANModus == F("ap"));
    if(wifiAp != neuerWifiAp) {
      wifiAp = neuerWifiAp;
      wlanAenderungVorgenommen = true;
      logger.info(F("WLAN Modus wird geändert auf: ") + String(wifiAp ? F("Access Point") : F("WLAN Client")));
    }
  }

  AktualisiereString(F("logLevel"), logLevel);
  AktualisiereBoolean(F("logInDatei"), logInDatei);

  #if MODUL_LEDAMPEL
    AktualisiereInteger(F("ampelModus"), ampelModus);
    AktualisiereBoolean(F("ampelAn"), ampelAn);
  #endif

  #if MODUL_DISPLAY
    AktualisiereInteger(F("status"), status);
    AktualisiereBoolean(F("displayAn"), displayAn);
  #endif

  #if MODUL_DHT
    AktualisiereBoolean(F("lufttemperaturWebhook"), lufttemperaturWebhook);
    AktualisiereInteger(F("lufttemperaturGruenUnten"), lufttemperaturGruenUnten);
    AktualisiereInteger(F("lufttemperaturGruenOben"), lufttemperaturGruenOben);
    AktualisiereInteger(F("lufttemperaturGelbUnten"), lufttemperaturGelbUnten);
    AktualisiereInteger(F("lufttemperaturGelbOben"), lufttemperaturGelbOben);
    AktualisiereBoolean(F("luftfeuchteWebhook"), luftfeuchteWebhook);
    AktualisiereInteger(F("luftfeuchteGruenUnten"), luftfeuchteGruenUnten);
    AktualisiereInteger(F("luftfeuchteGruenOben"), luftfeuchteGruenOben);
    AktualisiereInteger(F("luftfeuchteGelbUnten"), luftfeuchteGelbUnten);
    AktualisiereInteger(F("luftfeuchteGelbOben"), luftfeuchteGelbOben);
  #endif

  #if MODUL_WEBHOOK
    AktualisiereBoolean(F("webhookAn"), webhookAn);
    AktualisiereString(F("webhookDomain"), webhookDomain);
    AktualisiereString(F("webhookPfad"), webhookPfad);
    AktualisiereInteger(F("webhookFrequenz"), webhookFrequenz);
    AktualisiereInteger(F("webhookPingFrequenz"), webhookPingFrequenz);
  #endif

  #if MODUL_WIFI
    // WLAN Einstellungen immer aktualisieren
    AktualisiereString(F("wifiSsid1"), wifiSsid1, true);
    AktualisiereString(F("wifiPasswort1"), wifiPasswort1, true);
    AktualisiereString(F("wifiSsid2"), wifiSsid2, true);
    AktualisiereString(F("wifiPasswort2"), wifiPasswort2, true);
    AktualisiereString(F("wifiSsid3"), wifiSsid3, true);
    AktualisiereString(F("wifiPasswort3"), wifiPasswort3, true);

    AktualisiereString(F("wifiApSsid"), wifiApSsid, true);
    AktualisiereBoolean(F("wifiApPasswortAktiviert"), wifiApPasswortAktiviert, true);
    if(wifiApPasswortAktiviert) {
      AktualisiereString(F("wifiApPasswort"), wifiApPasswort, true);
    }
  #endif

  #if MODUL_HELLIGKEIT
    AktualisiereString(F("helligkeitName"), helligkeitName);
    AktualisiereBoolean(F("helligkeitWebhook"), helligkeitWebhook);
    AktualisiereInteger(F("helligkeitMinimum"), helligkeitMinimum);
    AktualisiereInteger(F("helligkeitMaximum"), helligkeitMaximum);
    AktualisiereInteger(F("helligkeitGruenUnten"), helligkeitGruenUnten);
    AktualisiereInteger(F("helligkeitGruenOben"), helligkeitGruenOben);
    AktualisiereInteger(F("helligkeitGelbUnten"), helligkeitGelbUnten);
    AktualisiereInteger(F("helligkeitGelbOben"), helligkeitGelbOben);
  #endif

  #if MODUL_BODENFEUCHTE
    AktualisiereString(F("bodenfeuchteName"), bodenfeuchteName);
    AktualisiereBoolean(F("bodenfeuchteWebhook"), bodenfeuchteWebhook);
    AktualisiereInteger(F("bodenfeuchteMinimum"), bodenfeuchteMinimum);
    AktualisiereInteger(F("bodenfeuchteMaximum"), bodenfeuchteMaximum);
    AktualisiereInteger(F("bodenfeuchteGruenUnten"), bodenfeuchteGruenUnten);
    AktualisiereInteger(F("bodenfeuchteGruenOben"), bodenfeuchteGruenOben);
    AktualisiereInteger(F("bodenfeuchteGelbUnten"), bodenfeuchteGelbUnten);
    AktualisiereInteger(F("bodenfeuchteGelbOben"), bodenfeuchteGelbOben);
  #endif

  #if MODUL_ANALOG3
    AktualisiereAnalogsensor(3);
  #endif
  #if MODUL_ANALOG4
    AktualisiereAnalogsensor(4);
  #endif
  #if MODUL_ANALOG5
    AktualisiereAnalogsensor(5);
  #endif
  #if MODUL_ANALOG6
    AktualisiereAnalogsensor(6);
  #endif
  #if MODUL_ANALOG7
    AktualisiereAnalogsensor(7);
  #endif
  #if MODUL_ANALOG8
    AktualisiereAnalogsensor(8);
  #endif
}

void AktualisiereAnalogsensor(int sensorNumber) {
  String prefix = F("analog") + String(sensorNumber);

  switch(sensorNumber) {
    #if MODUL_ANALOG3
      case 3:
        AktualisiereString(prefix + F("Name"), analog3Name);
        AktualisiereBoolean(prefix + F("Webhook"), analog3Webhook);
        AktualisiereInteger(prefix + F("Minimum"), analog3Minimum);
        AktualisiereInteger(prefix + F("Maximum"), analog3Maximum);
        AktualisiereInteger(prefix + F("GruenUnten"), analog3GruenUnten);
        AktualisiereInteger(prefix + F("GruenOben"), analog3GruenOben);
        AktualisiereInteger(prefix + F("GelbUnten"), analog3GelbUnten);
        AktualisiereInteger(prefix + F("GelbOben"), analog3GelbOben);
        break;
    #endif
    #if MODUL_ANALOG4
      case 4:
        AktualisiereString(prefix + F("Name"), analog4Name);
        AktualisiereBoolean(prefix + F("Webhook"), analog4Webhook);
        AktualisiereInteger(prefix + F("Minimum"), analog4Minimum);
        AktualisiereInteger(prefix + F("Maximum"), analog4Maximum);
        AktualisiereInteger(prefix + F("GruenUnten"), analog4GruenUnten);
        AktualisiereInteger(prefix + F("GruenOben"), analog4GruenOben);
        AktualisiereInteger(prefix + F("GelbUnten"), analog4GelbUnten);
        AktualisiereInteger(prefix + F("GelbOben"), analog4GelbOben);
        break;
    #endif
    #if MODUL_ANALOG5
      case 5:
        AktualisiereString(prefix + F("Name"), analog5Name);
        AktualisiereBoolean(prefix + F("Webhook"), analog5Webhook);
        AktualisiereInteger(prefix + F("Minimum"), analog5Minimum);
        AktualisiereInteger(prefix + F("Maximum"), analog5Maximum);
        AktualisiereInteger(prefix + F("GruenUnten"), analog5GruenUnten);
        AktualisiereInteger(prefix + F("GruenOben"), analog5GruenOben);
        AktualisiereInteger(prefix + F("GelbUnten"), analog5GelbUnten);
        AktualisiereInteger(prefix + F("GelbOben"), analog5GelbOben);
        break;
    #endif
    #if MODUL_ANALOG6
      case 6:
        AktualisiereString(prefix + F("Name"), analog6Name);
        AktualisiereBoolean(prefix + F("Webhook"), analog6Webhook);
        AktualisiereInteger(prefix + F("Minimum"), analog6Minimum);
        AktualisiereInteger(prefix + F("Maximum"), analog6Maximum);
        AktualisiereInteger(prefix + F("GruenUnten"), analog6GruenUnten);
        AktualisiereInteger(prefix + F("GruenOben"), analog6GruenOben);
        AktualisiereInteger(prefix + F("GelbUnten"), analog6GelbUnten);
        AktualisiereInteger(prefix + F("GelbOben"), analog6GelbOben);
        break;
    #endif
    #if MODUL_ANALOG7
      case 7:
        AktualisiereString(prefix + F("Name"), analog7Name);
        AktualisiereBoolean(prefix + F("Webhook"), analog7Webhook);
        AktualisiereInteger(prefix + F("Minimum"), analog7Minimum);
        AktualisiereInteger(prefix + F("Maximum"), analog7Maximum);
        AktualisiereInteger(prefix + F("GruenUnten"), analog7GruenUnten);
        AktualisiereInteger(prefix + F("GruenOben"), analog7GruenOben);
        AktualisiereInteger(prefix + F("GelbUnten"), analog7GelbUnten);
        AktualisiereInteger(prefix + F("GelbOben"), analog7GelbOben);
        break;
    #endif
    #if MODUL_ANALOG8
      case 8:
        AktualisiereString(prefix + F("Name"), analog8Name);
        AktualisiereBoolean(prefix + F("Webhook"), analog8Webhook);
        AktualisiereInteger(prefix + F("Minimum"), analog8Minimum);
        AktualisiereInteger(prefix + F("Maximum"), analog8Maximum);
        AktualisiereInteger(prefix + F("GruenUnten"), analog8GruenUnten);
        AktualisiereInteger(prefix + F("GruenOben"), analog8GruenOben);
        AktualisiereInteger(prefix + F("GelbUnten"), analog8GelbUnten);
        AktualisiereInteger(prefix + F("GelbOben"), analog8GelbOben);
        break;
    #endif
  }
}

void AktualisiereString(const String& argName, String& wert, bool istWLANEinstellung) {
  if (Webserver.arg(argName) != F("")) {
    String neuerWert = Webserver.arg(argName);
    if (neuerWert != wert) {
      wert = neuerWert;
      if (istWLANEinstellung) {
        wlanAenderungVorgenommen = true;
      }
    }
  }
}

void AktualisiereInteger(const String& argName, int& wert, bool istWLANEinstellung) {
  if (Webserver.arg(argName) != F("")) {
    int neuerWert = Webserver.arg(argName).toInt();
    if (neuerWert != wert) {
      wert = neuerWert;
      if (istWLANEinstellung) {
        wlanAenderungVorgenommen = true;
      }
    }
  }
}

void AktualisiereBoolean(const String& argName, bool& wert, bool istWLANEinstellung) {
  bool neuerWert = Webserver.hasArg(argName);
  if (neuerWert != wert) {
    wert = neuerWert;
    if (istWLANEinstellung) {
      wlanAenderungVorgenommen = true;
    }
  }
}
