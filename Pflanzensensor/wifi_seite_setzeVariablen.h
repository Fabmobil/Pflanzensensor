// Funktionsdeklarationen
void ArgumenteAusgeben();
void WebseiteSetzeVariablen();
void AktualisiereVariablen();
void AktualisiereAnalogsensor(int sensorNumber);
void AktualisiereBoolean(const String& argName, bool& wert);
void AktualisiereInteger(const String& argName, int& wert);
void AktualisiereString(const String& argName, String& wert);

/* Funktion: ArgumenteAusgeben()
 * Gibt alle Argumente aus, die übergeben wurden.
 */
void ArgumenteAusgeben() {
  Serial.println(F("Gebe alle Argumente des POST requests aus:"));
  int numArgs = Webserver.args();
  for (int i = 0; i < numArgs; i++) {
    Serial.print(Webserver.argName(i));
    Serial.print(F(": "));
    Serial.println(Webserver.arg(i));
  }
}

/*
 * Funktion: Void WebseiteSetzeVariablen()
 * Übernimmt die Änderungen, welche auf der Administrationsseite gemacht wurden.
 */
void WebseiteSetzeVariablen() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebseiteSetzeVariablen()"));
    ArgumenteAusgeben();
  #endif

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeader);

  if (Webserver.arg("Passwort") == wifiAdminPasswort) {
    AktualisiereVariablen();
    Webserver.sendContent(F("<h3>Erfolgreich!</h3>\n"));
  } else {
    Webserver.sendContent(F("<h3>Falsches Passwort!</h3>\n"));
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
    Webserver.sendContent(F(
      "<div class=\"tuerkis\">\n"
      "<ul>\n"
      "<li><a href=\"/\">zur Startseite</a></li>\n"
      "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"
    ));
    #if MODUL_DEBUG
      Webserver.sendContent(F("<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"));
    #endif
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

void AktualisiereInteger(const String& argName, int& wert) {
  if (Webserver.arg(argName) != "") {
    wert = Webserver.arg(argName).toInt();
  }
}

void AktualisiereString(const String& argName, String& wert) {
  if (Webserver.arg(argName) != "") {
    wert = Webserver.arg(argName);
  }
}

void AktualisiereBoolean(const String& argName, bool& wert) {
  if (Webserver.hasArg(argName)) {
    wert = true;
  } else {
    wert = false;
  }
}
