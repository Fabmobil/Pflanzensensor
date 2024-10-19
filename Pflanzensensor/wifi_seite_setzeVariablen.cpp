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
#include "variablenspeicher.h"
#include "wifi.h"
#include "wifi_footer.h"
#include "wifi_header.h"
#include "logger.h"

void ArgumenteAusgeben() {
  logger.info("Gebe alle Argumente des POST requests aus:");
  int numArgs = Webserver.args();
  for (int i = 0; i < numArgs; i++) {
    logger.info(String(Webserver.argName(i)) + ": " + String(Webserver.arg(i)));
  }
}

void WebseiteSetzeVariablen() {
    Serial.println(F("Beginn von WebseiteSetzeVariablen()"));

    millisVorherWebhook = millisAktuell; // Webhook löst sonst sofort aus und gemeinsam mit dem Variablen setzen führt dazu, dass der ESP abstürzt.

    Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
    Webserver.send(200, F("text/html"), "");

    Webserver.sendContent_P(htmlHeaderNoRefresh);
    Webserver.sendContent_P(htmlHeader);

    if (Webserver.arg("Passwort") == wifiAdminPasswort) {
        String aenderungen = "<ul>\n"; // Hier sammeln wir alle Änderungen

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

        // Wir überprüfen jedes Eingabefeld und fügen Änderungen hinzu, wenn etwas geändert wurde
        for (int i = 0; i < Webserver.args(); i++) {
            String argName = Webserver.argName(i);
            String argValue = Webserver.arg(i);

            if (argName != "Passwort") {
                // Spezieller Fall für den WLAN-Modus
                if (argName == "wlanModus") {
                    bool neuerWlanAp = (argValue == "ap");
                    if (neuerWlanAp != wifiAp) {
                        wlanAenderungVorgenommen = true;
                        String neuerModus = neuerWlanAp ? "Access Point" : "WLAN Client";
                        aenderungen += "<li>WLAN-Modus: " + neuerModus + "</li>\n";
                    }
                }
                // Bei Checkboxen vergleichen wir mit dem alten Zustand
                else if (argName.endsWith("Webhook") || argName == "ampelAn" || argName == "displayAn" || argName == "webhookAn") {
                  int index = -1;
                  if (argName == "bodenfeuchteWebhook") index = 0;
                  else if (argName == "helligkeitWebhook") index = 1;
                  else if (argName == "lufttemperaturWebhook") index = 2;
                  else if (argName == "luftfeuchteWebhook") index = 3;
                  else if (argName == "ampelAn") index = 4;
                  else if (argName == "displayAn") index = 5;
                  else if (argName == "webhookAn") index = 6;
                  else if (argName == "logLevel") index = 7;

                  if (index != -1) {
                      bool neuerZustand = Webserver.hasArg(argName);
                      if (neuerZustand != alteCheckboxZustaende[index]) {
                          aenderungen += "<li>" + argName + ": " + (neuerZustand ? "aktiviert" : "deaktiviert") + "</li>\n";
                      }
                  }
              }
                // Für alle anderen Felder
                else if (argValue != "") {
                    aenderungen += "<li>" + argName + ": " + argValue + "</li>\n";
                }
            }
        }

        // Überprüfen, ob Checkboxen deaktiviert wurden
        String checkboxNamen[] = {"bodenfeuchteWebhook", "helligkeitWebhook", "lufttemperaturWebhook", "luftfeuchteWebhook", "ampelAn", "displayAn"};
        for (int i = 0; i < 6; i++) {
            if (alteCheckboxZustaende[i] && !Webserver.hasArg(checkboxNamen[i])) {
                aenderungen += "<li>" + checkboxNamen[i] + ": deaktiviert</li>\n";
            }
        }

        aenderungen += "</ul>\n";

        // Jetzt aktualisieren wir die Variablen
        AktualisiereVariablen();
        Webserver.sendContent(F("<h3>Erfolgreich!</h3>\n"));
        Webserver.sendContent(F("<div class=\"gruen\">\n"));

        if (aenderungen == "<ul>\n</ul>\n") {
            Webserver.sendContent(F("<p>Es wurden keine Änderungen vorgenommen.</p>\n"));
        } else {
            Webserver.sendContent(F("<p>Folgende Änderungen wurden vorgenommen:</p>\n"));
            Webserver.sendContent(aenderungen);
        }

        Webserver.sendContent(F("</div>"));
        if (wlanAenderungVorgenommen) {
            Webserver.sendContent(F("<h3>Achtung!</h3>\n<div class=\"rot\">\n"));
            Webserver.sendContent(F("<p>Es wurden WLAN Daten geändert.\n"));
            Webserver.sendContent(F("Die WLAN Verbindung des Pflanzensensors wird deshalb in Kürze neu starten, um die Änderungen zu übernehmen."));
            Webserver.sendContent(F("Gegebenenfalls ändert sich die SSID und die IP Adresse deines Sensors. Achte auf das Display!</p>\n</div>"));

        }
    } else {
        Webserver.sendContent(F("<h3>Falsches Passwort!</h3>\n<div class=\"rot\">\n"));
        Webserver.sendContent(F("<p>Du hast nicht das richtige Passwort eingebeben!</p></div>\n"));
    }

    if (Webserver.arg("loeschen") == "Ja!") {
        Webserver.sendContent(F(
            "<div class=\"rot\">\n"
            "<p>Alle Variablen wurden gelöscht.</p>\n"
            "<p>Der Pflanzensensor wird neu gestartet.</p>\n"
            "</div>\n"
            "<div class=\"tuerkis\">\n"
            "<p><a href=\"/\">Warte ein paar Sekunden, dann kannst du hier zur Startseite zurück.</a></p>\n"
            "</div>\n"
        ));
        Webserver.sendContent_P(htmlFooter);
        Webserver.client().flush();
        VariablenLoeschen();
        delay(5);
        ESP.restart();
    } else {
        Webserver.sendContent(F("<h3>Links</h3>\n"));
        Webserver.sendContent(F(
            "<div class=\"tuerkis\">\n"
            "<ul>\n"
            "<li><a href=\"/\">zur Startseite</a></li>\n"
            "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"
        ));
        Webserver.sendContent(F("<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"));

        Webserver.sendContent(F(
            "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
            "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
            "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
            "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
            "</ul>\n"
            "</div>\n"
        ));
        Webserver.sendContent_P(htmlFooter);
        Webserver.client().flush();
        VariablenSpeichern();
    }
}

void AktualisiereVariablen() {
  AktualisiereString("logLevel", logLevel);
  AktualisiereInteger("logAnzahlEintraege", logAnzahlEintraege);
  AktualisiereInteger("logAnzahlWebseite", logAnzahlWebseite);
  AktualisiereBoolean("logInDatei", logInDatei);
  #if MODUL_LEDAMPEL
    AktualisiereInteger("ampelModus", ampelModus);
    AktualisiereBoolean("ampelAn", ampelAn);
  #endif

  #if MODUL_DISPLAY
    AktualisiereInteger("status", status);
    AktualisiereBoolean("displayAn", displayAn);
  #endif

  #if MODUL_DHT
    AktualisiereBoolean("lufttemperaturWebhook", lufttemperaturWebhook);
    AktualisiereInteger("lufttemperaturGruenUnten", lufttemperaturGruenUnten);
    AktualisiereInteger("lufttemperaturGruenOben", lufttemperaturGruenOben);
    AktualisiereInteger("lufttemperaturGelbUnten", lufttemperaturGelbUnten);
    AktualisiereInteger("lufttemperaturGelbOben", lufttemperaturGelbOben);
    AktualisiereBoolean("luftfeuchteWebhook", luftfeuchteWebhook);
    AktualisiereInteger("luftfeuchteGruenUnten", luftfeuchteGruenUnten);
    AktualisiereInteger("luftfeuchteGruenOben", luftfeuchteGruenOben);
    AktualisiereInteger("luftfeuchteGelbUnten", luftfeuchteGelbUnten);
    AktualisiereInteger("luftfeuchteGelbOben", luftfeuchteGelbOben);
  #endif

  #if MODUL_WEBHOOK
    AktualisiereBoolean("webhookAn", webhookAn);
    AktualisiereString("webhookDomain", webhookDomain);
    AktualisiereString("webhookPfad", webhookPfad);
    AktualisiereInteger("webhookFrequenz", webhookFrequenz);
    AktualisiereInteger("webhookPingFrequenz", webhookPingFrequenz);
  #endif

  #if MODUL_WIFI
    wlanAenderungVorgenommen = false;
    String neuerWLANModus = Webserver.arg("wlanModus");
    if ((neuerWLANModus == "ap" && !wifiAp) || (neuerWLANModus == "wlan" && wifiAp)) {
      wifiAp = (neuerWLANModus == "ap");
      wlanAenderungVorgenommen = true;
    }

    AktualisiereString("wifiSsid1", wifiSsid1, true);
    AktualisiereString("wifiPassword1", wifiPassword1, true);
    AktualisiereString("wifiSsid2", wifiSsid2, true);
    AktualisiereString("wifiPassword2", wifiPassword2, true);
    AktualisiereString("wifiSsid3", wifiSsid3, true);
    AktualisiereString("wifiPassword3", wifiPassword3, true);
    AktualisiereString("wifiApSsid", wifiApSsid, true);
    AktualisiereBoolean("wifiApPasswortAktiviert", wifiApPasswortAktiviert, true);
    if (wifiApPasswortAktiviert) {
      AktualisiereString("wifiApPasswort", wifiApPasswort, true);
    }

    if (wlanAenderungVorgenommen) {
      VerzoegerterWLANNeustart(); // Plane einen verzögerten WLAN-Neustart
    }
  #endif

  #if MODUL_HELLIGKEIT
    AktualisiereString("helligkeitName", helligkeitName);
    AktualisiereBoolean("helligkeitWebhook", helligkeitWebhook);
    AktualisiereInteger("helligkeitMinimum", helligkeitMinimum);
    AktualisiereInteger("helligkeitMaximum", helligkeitMaximum);
    AktualisiereInteger("helligkeitGruenUnten", helligkeitGruenUnten);
    AktualisiereInteger("helligkeitGruenOben", helligkeitGruenOben);
    AktualisiereInteger("helligkeitGelbUnten", helligkeitGelbUnten);
    AktualisiereInteger("helligkeitGelbOben", helligkeitGelbOben);
  #endif

  #if MODUL_BODENFEUCHTE
    AktualisiereString("bodenfeuchteName", bodenfeuchteName);
    AktualisiereBoolean("bodenfeuchteWebhook", bodenfeuchteWebhook);
    AktualisiereInteger("bodenfeuchteMinimum", bodenfeuchteMinimum);
    AktualisiereInteger("bodenfeuchteMaximum", bodenfeuchteMaximum);
    AktualisiereInteger("bodenfeuchteGruenUnten", bodenfeuchteGruenUnten);
    AktualisiereInteger("bodenfeuchteGruenOben", bodenfeuchteGruenOben);
    AktualisiereInteger("bodenfeuchteGelbUnten", bodenfeuchteGelbUnten);
    AktualisiereInteger("bodenfeuchteGelbOben", bodenfeuchteGelbOben);
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
  String prefix = "analog" + String(sensorNumber);

  switch(sensorNumber) {
    #if MODUL_ANALOG3
      case 3:
        AktualisiereString(prefix + "Name", analog3Name);
        AktualisiereBoolean(prefix + "Webhook", analog3Webhook);
        AktualisiereInteger(prefix + "Minimum", analog3Minimum);
        AktualisiereInteger(prefix + "Maximum", analog3Maximum);
        AktualisiereInteger(prefix + "GruenUnten", analog3GruenUnten);
        AktualisiereInteger(prefix + "GruenOben", analog3GruenOben);
        AktualisiereInteger(prefix + "GelbUnten", analog3GelbUnten);
        AktualisiereInteger(prefix + "GelbOben", analog3GelbOben);
        break;
    #endif
    #if MODUL_ANALOG4
      case 4:
        AktualisiereString(prefix + "Name", analog4Name);
        AktualisiereBoolean(prefix + "Webhook", analog4Webhook);
        AktualisiereInteger(prefix + "Minimum", analog4Minimum);
        AktualisiereInteger(prefix + "Maximum", analog4Maximum);
        AktualisiereInteger(prefix + "GruenUnten", analog4GruenUnten);
        AktualisiereInteger(prefix + "GruenOben", analog4GruenOben);
        AktualisiereInteger(prefix + "GelbUnten", analog4GelbUnten);
        AktualisiereInteger(prefix + "GelbOben", analog4GelbOben);
        break;
    #endif
    #if MODUL_ANALOG5
      case 5:
        AktualisiereString(prefix + "Name", analog5Name);
        AktualisiereBoolean(prefix + "Webhook", analog5Webhook);
        AktualisiereInteger(prefix + "Minimum", analog5Minimum);
        AktualisiereInteger(prefix + "Maximum", analog5Maximum);
        AktualisiereInteger(prefix + "GruenUnten", analog5GruenUnten);
        AktualisiereInteger(prefix + "GruenOben", analog5GruenOben);
        AktualisiereInteger(prefix + "GelbUnten", analog5GelbUnten);
        AktualisiereInteger(prefix + "GelbOben", analog5GelbOben);
        break;
    #endif
    #if MODUL_ANALOG6
      case 6:
        AktualisiereString(prefix + "Name", analog6Name);
        AktualisiereBoolean(prefix + "Webhook", analog6Webhook);
        AktualisiereInteger(prefix + "Minimum", analog6Minimum);
        AktualisiereInteger(prefix + "Maximum", analog6Maximum);
        AktualisiereInteger(prefix + "GruenUnten", analog6GruenUnten);
        AktualisiereInteger(prefix + "GruenOben", analog6GruenOben);
        AktualisiereInteger(prefix + "GelbUnten", analog6GelbUnten);
        AktualisiereInteger(prefix + "GelbOben", analog6GelbOben);
        break;
    #endif
    #if MODUL_ANALOG7
      case 7:
        AktualisiereString(prefix + "Name", analog7Name);
        AktualisiereBoolean(prefix + "Webhook", analog7Webhook);
        AktualisiereInteger(prefix + "Minimum", analog7Minimum);
        AktualisiereInteger(prefix + "Maximum", analog7Maximum);
        AktualisiereInteger(prefix + "GruenUnten", analog7GruenUnten);
        AktualisiereInteger(prefix + "GruenOben", analog7GruenOben);
        AktualisiereInteger(prefix + "GelbUnten", analog7GelbUnten);
        AktualisiereInteger(prefix + "GelbOben", analog7GelbOben);
        break;
    #endif
    #if MODUL_ANALOG8
      case 8:
        AktualisiereString(prefix + "Name", analog8Name);
        AktualisiereBoolean(prefix + "Webhook", analog8Webhook);
        AktualisiereInteger(prefix + "Minimum", analog8Minimum);
        AktualisiereInteger(prefix + "Maximum", analog8Maximum);
        AktualisiereInteger(prefix + "GruenUnten", analog8GruenUnten);
        AktualisiereInteger(prefix + "GruenOben", analog8GruenOben);
        AktualisiereInteger(prefix + "GelbUnten", analog8GelbUnten);
        AktualisiereInteger(prefix + "GelbOben", analog8GelbOben);
        break;
    #endif
  }
}

void AktualisiereString(const String& argName, String& wert, bool istWLANEinstellung) {
  if (Webserver.arg(argName) != "") {
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
  if (Webserver.arg(argName) != "") {
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
