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
  AktualisiereString("logLevel", logLevel, sizeof(logLevel));
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
    AktualisiereString("webhookDomain", webhookDomain, sizeof(webhookDomain));
    AktualisiereString("webhookPfad", webhookPfad, sizeof(webhookPfad));
    AktualisiereInteger("webhookFrequenz", webhookFrequenz);
    AktualisiereInteger("webhookPingFrequenz", webhookPingFrequenz);
  #endif

  #if MODUL_WIFI
    wlanAenderungVorgenommen = false;
    if (Webserver.hasArg("wlanModus")) {
      const char* neuerWLANModus = Webserver.arg("wlanModus").c_str();
      if ((strcmp(neuerWLANModus, "ap") == 0 && !wifiAp) || (strcmp(neuerWLANModus, "wlan") == 0 && wifiAp)) {
        wifiAp = (strcmp(neuerWLANModus, "ap") == 0);
        wlanAenderungVorgenommen = true;
      }
    }

    AktualisiereString("wifiSsid1", wifiSsid1, sizeof(wifiSsid1), true);
    AktualisiereString("wifiPasswort1", wifiPasswort1, sizeof(wifiPasswort1), true);
    AktualisiereString("wifiSsid2", wifiSsid2, sizeof(wifiSsid2), true);
    AktualisiereString("wifiPasswort2", wifiPasswort2, sizeof(wifiPasswort2), true);
    AktualisiereString("wifiSsid3", wifiSsid3, sizeof(wifiSsid3), true);
    AktualisiereString("wifiPasswort3", wifiPasswort3, sizeof(wifiPasswort3), true);
    AktualisiereString("wifiApSsid", wifiApSsid, sizeof(wifiApSsid), true);
    AktualisiereBoolean("wifiApPasswortAktiviert", wifiApPasswortAktiviert, true);
    if (wifiApPasswortAktiviert) {
      AktualisiereString("wifiApPasswort", wifiApPasswort, sizeof(wifiApPasswort), true);
    }

    if (wlanAenderungVorgenommen) {
      VerzoegerterWLANNeustart(); // Plane einen verzögerten WLAN-Neustart
    }
  #endif

  #if MODUL_HELLIGKEIT
    AktualisiereString("helligkeitName", helligkeitName, sizeof(helligkeitName));
    AktualisiereBoolean("helligkeitWebhook", helligkeitWebhook);
    AktualisiereInteger("helligkeitMinimum", helligkeitMinimum);
    AktualisiereInteger("helligkeitMaximum", helligkeitMaximum);
    AktualisiereInteger("helligkeitGruenUnten", helligkeitGruenUnten);
    AktualisiereInteger("helligkeitGruenOben", helligkeitGruenOben);
    AktualisiereInteger("helligkeitGelbUnten", helligkeitGelbUnten);
    AktualisiereInteger("helligkeitGelbOben", helligkeitGelbOben);
  #endif

  #if MODUL_BODENFEUCHTE
    AktualisiereString("bodenfeuchteName", bodenfeuchteName, sizeof(bodenfeuchteName));
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
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "analog%d", sensorNumber);

  char nameBuf[32];
  char webhookBuf[32];
  char minimumBuf[32];
  char maximumBuf[32];
  char gruenUntenBuf[32];
  char gruenObenBuf[32];
  char gelbUntenBuf[32];
  char gelbObenBuf[32];

  snprintf(nameBuf, sizeof(nameBuf), "%sName", prefix);
  snprintf(webhookBuf, sizeof(webhookBuf), "%sWebhook", prefix);
  snprintf(minimumBuf, sizeof(minimumBuf), "%sMinimum", prefix);
  snprintf(maximumBuf, sizeof(maximumBuf), "%sMaximum", prefix);
  snprintf(gruenUntenBuf, sizeof(gruenUntenBuf), "%sGruenUnten", prefix);
  snprintf(gruenObenBuf, sizeof(gruenObenBuf), "%sGruenOben", prefix);
  snprintf(gelbUntenBuf, sizeof(gelbUntenBuf), "%sGelbUnten", prefix);
  snprintf(gelbObenBuf, sizeof(gelbObenBuf), "%sGelbOben", prefix);

  switch(sensorNumber) {
    #if MODUL_ANALOG3
      case 3:
        AktualisiereString(nameBuf, analog3Name, sizeof(analog3Name));
        AktualisiereBoolean(webhookBuf, analog3Webhook);
        AktualisiereInteger(minimumBuf, analog3Minimum);
        AktualisiereInteger(maximumBuf, analog3Maximum);
        AktualisiereInteger(gruenUntenBuf, analog3GruenUnten);
        AktualisiereInteger(gruenObenBuf, analog3GruenOben);
        AktualisiereInteger(gelbUntenBuf, analog3GelbUnten);
        AktualisiereInteger(gelbObenBuf, analog3GelbOben);
        break;
    #endif
    #if MODUL_ANALOG4
      case 4:
        AktualisiereString(nameBuf, analog4Name, sizeof(analog4Name));
        AktualisiereBoolean(webhookBuf, analog4Webhook);
        AktualisiereInteger(minimumBuf, analog4Minimum);
        AktualisiereInteger(maximumBuf, analog4Maximum);
        AktualisiereInteger(gruenUntenBuf, analog4GruenUnten);
        AktualisiereInteger(gruenObenBuf, analog4GruenOben);
        AktualisiereInteger(gelbUntenBuf, analog4GelbUnten);
        AktualisiereInteger(gelbObenBuf, analog4GelbOben);
        break;
    #endif
    #if MODUL_ANALOG5
      case 5:
        AktualisiereString(nameBuf, analog5Name, sizeof(analog5Name));
        AktualisiereBoolean(webhookBuf, analog5Webhook);
        AktualisiereInteger(minimumBuf, analog5Minimum);
        AktualisiereInteger(maximumBuf, analog5Maximum);
        AktualisiereInteger(gruenUntenBuf, analog5GruenUnten);
        AktualisiereInteger(gruenObenBuf, analog5GruenOben);
        AktualisiereInteger(gelbUntenBuf, analog5GelbUnten);
        AktualisiereInteger(gelbObenBuf, analog5GelbOben);
        break;
    #endif
    #if MODUL_ANALOG6
      case 6:
        AktualisiereString(nameBuf, analog6Name, sizeof(analog6Name));
        AktualisiereBoolean(webhookBuf, analog6Webhook);
        AktualisiereInteger(minimumBuf, analog6Minimum);
        AktualisiereInteger(maximumBuf, analog6Maximum);
        AktualisiereInteger(gruenUntenBuf, analog6GruenUnten);
        AktualisiereInteger(gruenObenBuf, analog6GruenOben);
        AktualisiereInteger(gelbUntenBuf, analog6GelbUnten);
        AktualisiereInteger(gelbObenBuf, analog6GelbOben);
        break;
    #endif
    #if MODUL_ANALOG7
      case 7:
        AktualisiereString(nameBuf, analog7Name, sizeof(analog7Name));
        AktualisiereBoolean(webhookBuf, analog7Webhook);
        AktualisiereInteger(minimumBuf, analog7Minimum);
        AktualisiereInteger(maximumBuf, analog7Maximum);
        AktualisiereInteger(gruenUntenBuf, analog7GruenUnten);
        AktualisiereInteger(gruenObenBuf, analog7GruenOben);
        AktualisiereInteger(gelbUntenBuf, analog7GelbUnten);
        AktualisiereInteger(gelbObenBuf, analog7GelbOben);
        break;
    #endif
    #if MODUL_ANALOG8
      case 8:
        AktualisiereString(nameBuf, analog8Name, sizeof(analog8Name));
        AktualisiereBoolean(webhookBuf, analog8Webhook);
        AktualisiereInteger(minimumBuf, analog8Minimum);
        AktualisiereInteger(maximumBuf, analog8Maximum);
        AktualisiereInteger(gruenUntenBuf, analog8GruenUnten);
        AktualisiereInteger(gruenObenBuf, analog8GruenOben);
        AktualisiereInteger(gelbUntenBuf, analog8GelbUnten);
        AktualisiereInteger(gelbObenBuf, analog8GelbOben);
        break;
    #endif
  }
}

void AktualisiereString(const char* argName, char* wert, size_t maxLength, bool istWLANEinstellung) {
  if (Webserver.hasArg(argName)) {
    const String& neuerWert = Webserver.arg(argName);
    if (strncmp(neuerWert.c_str(), wert, maxLength) != 0) {
      strncpy(wert, neuerWert.c_str(), maxLength - 1);
      wert[maxLength - 1] = '\0';
      if (istWLANEinstellung) {
        wlanAenderungVorgenommen = true;
      }
    }
  }
}

void AktualisiereInteger(const char* argName, int& wert, bool istWLANEinstellung) {
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

void AktualisiereBoolean(const char* argName, bool& wert, bool istWLANEinstellung) {
  bool neuerWert = Webserver.hasArg(argName);
  if (neuerWert != wert) {
    wert = neuerWert;
    if (istWLANEinstellung) {
      wlanAenderungVorgenommen = true;
    }
  }
}

