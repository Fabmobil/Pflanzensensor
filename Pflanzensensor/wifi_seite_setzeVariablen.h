// Funktionsdeklarationen
void ArgumenteAusgeben();
void WebseiteSetzeVariablen();
void updateVariables();
void updateAnalogSensor(int sensorNumber);
void updateBoolValue(const String& argName, bool& value);
void updateIntValue(const String& argName, int& value);
void updateStringValue(const String& argName, String& value);
void sendLinks();

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
    updateVariables();
    Webserver.sendContent(F("<h2>Erfolgreich!</h2>\n"));
  } else {
    Webserver.sendContent(F("<h2>Falsches Passwort!</h2>\n"));
  }

  if (Webserver.arg("loeschen") == "Ja!") {
    Webserver.sendContent(F(
      "<div class=\"rot\">\n"
      "<p>Alle Variablen wurden gelöscht.</p>\n"
      "<p>Der Pflanzensensor wird neu gestartet.</p>\n"
      "</div>\n"
      "<div class=\"weiss\">\n"
      "<p><a href=\"/\">Warte ein paar Sekunden, dann kannst du hier zur Startseite zurück.</a></p>\n"
      "</div>\n"
    ));
  } else {
    sendLinks();
  }

  Webserver.sendContent_P(htmlFooter);

  if (Webserver.arg("loeschen") == "Ja!") {
    VariablenLoeschen();
    ESP.restart();
  } else {
    VariablenSpeichern();
  }
}

void updateVariables() {
  #if MODUL_LEDAMPEL
    updateIntValue("ampelModus", ampelModus);
    updateBoolValue("ampelAn", ampelAn);
  #endif

  #if MODUL_DISPLAY
    updateIntValue("status", status);
    updateBoolValue("displayAn", displayAn);
  #endif

  #if MODUL_DHT
    updateBoolValue("lufttemperaturWebhook", lufttemperaturWebhook);
    updateIntValue("lufttemperaturGruenUnten", lufttemperaturGruenUnten);
    updateIntValue("lufttemperaturGruenOben", lufttemperaturGruenOben);
    updateIntValue("lufttemperaturGelbUnten", lufttemperaturGelbUnten);
    updateIntValue("lufttemperaturGelbOben", lufttemperaturGelbOben);
    updateIntValue("luftfeuchteGruenUnten", luftfeuchteGruenUnten);
    updateIntValue("luftfeuchteGruenOben", luftfeuchteGruenOben);
    updateIntValue("luftfeuchteGelbUnten", luftfeuchteGelbUnten);
    updateIntValue("luftfeuchteGelbOben", luftfeuchteGelbOben);
  #endif

  #if MODUL_WEBHOOK
    updateBoolValue("webhookAn", webhookAn);
    updateStringValue("webhookDomain", webhookDomain);
    updateStringValue("webhookPfad", webhookPfad);
  #endif

  #if MODUL_HELLIGKEIT
    updateStringValue("helligkeitName", helligkeitName);
    updateBoolValue("helligkeitWebhook", helligkeitWebhook);
    updateIntValue("helligkeitMinimum", helligkeitMinimum);
    updateIntValue("helligkeitMaximum", helligkeitMaximum);
    updateIntValue("helligkeitGruenUnten", helligkeitGruenUnten);
    updateIntValue("helligkeitGruenOben", helligkeitGruenOben);
    updateIntValue("helligkeitGelbUnten", helligkeitGelbUnten);
    updateIntValue("helligkeitGelbOben", helligkeitGelbOben);
  #endif

  #if MODUL_BODENFEUCHTE
    updateStringValue("bodenfeuchteName", bodenfeuchteName);
    updateBoolValue("bodenfeuchteWebhook", bodenfeuchteWebhook);
    updateIntValue("bodenfeuchteMinimum", bodenfeuchteMinimum);
    updateIntValue("bodenfeuchteMaximum", bodenfeuchteMaximum);
    updateIntValue("bodenfeuchteGruenUnten", bodenfeuchteGruenUnten);
    updateIntValue("bodenfeuchteGruenOben", bodenfeuchteGruenOben);
    updateIntValue("bodenfeuchteGelbUnten", bodenfeuchteGelbUnten);
    updateIntValue("bodenfeuchteGelbOben", bodenfeuchteGelbOben);
  #endif

  #if MODUL_ANALOG3
    updateAnalogSensor(3);
  #endif
  #if MODUL_ANALOG4
    updateAnalogSensor(4);
  #endif
  #if MODUL_ANALOG5
    updateAnalogSensor(5);
  #endif
  #if MODUL_ANALOG6
    updateAnalogSensor(6);
  #endif
  #if MODUL_ANALOG7
    updateAnalogSensor(7);
  #endif
  #if MODUL_ANALOG8
    updateAnalogSensor(8);
  #endif
}

void updateAnalogSensor(int sensorNumber) {
  String prefix = "analog" + String(sensorNumber);

  switch(sensorNumber) {
    #if MODUL_ANALOG3
      case 3:
        updateStringValue(prefix + "Name", analog3Name);
        updateBoolValue(prefix + "Webhook", analog3Webhook);
        updateIntValue(prefix + "Minimum", analog3Minimum);
        updateIntValue(prefix + "Maximum", analog3Maximum);
        updateIntValue(prefix + "GruenUnten", analog3GruenUnten);
        updateIntValue(prefix + "GruenOben", analog3GruenOben);
        updateIntValue(prefix + "GelbUnten", analog3GelbUnten);
        updateIntValue(prefix + "GelbOben", analog3GelbOben);
        break;
    #endif
    #if MODUL_ANALOG4
      case 4:
        updateStringValue(prefix + "Name", analog4Name);
        updateBoolValue(prefix + "Webhook", analog4Webhook);
        updateIntValue(prefix + "Minimum", analog4Minimum);
        updateIntValue(prefix + "Maximum", analog4Maximum);
        updateIntValue(prefix + "GruenUnten", analog4GruenUnten);
        updateIntValue(prefix + "GruenOben", analog4GruenOben);
        updateIntValue(prefix + "GelbUnten", analog4GelbUnten);
        updateIntValue(prefix + "GelbOben", analog4GelbOben);
        break;
    #endif
    #if MODUL_ANALOG5
      case 5:
        updateStringValue(prefix + "Name", analog5Name);
        updateBoolValue(prefix + "Webhook", analog5Webhook);
        updateIntValue(prefix + "Minimum", analog5Minimum);
        updateIntValue(prefix + "Maximum", analog5Maximum);
        updateIntValue(prefix + "GruenUnten", analog5GruenUnten);
        updateIntValue(prefix + "GruenOben", analog5GruenOben);
        updateIntValue(prefix + "GelbUnten", analog5GelbUnten);
        updateIntValue(prefix + "GelbOben", analog5GelbOben);
        break;
    #endif
    #if MODUL_ANALOG6
      case 6:
        updateStringValue(prefix + "Name", analog6Name);
        updateBoolValue(prefix + "Webhook", analog6Webhook);
        updateIntValue(prefix + "Minimum", analog6Minimum);
        updateIntValue(prefix + "Maximum", analog6Maximum);
        updateIntValue(prefix + "GruenUnten", analog6GruenUnten);
        updateIntValue(prefix + "GruenOben", analog6GruenOben);
        updateIntValue(prefix + "GelbUnten", analog6GelbUnten);
        updateIntValue(prefix + "GelbOben", analog6GelbOben);
        break;
    #endif
    #if MODUL_ANALOG7
      case 7:
        updateStringValue(prefix + "Name", analog7Name);
        updateBoolValue(prefix + "Webhook", analog7Webhook);
        updateIntValue(prefix + "Minimum", analog7Minimum);
        updateIntValue(prefix + "Maximum", analog7Maximum);
        updateIntValue(prefix + "GruenUnten", analog7GruenUnten);
        updateIntValue(prefix + "GruenOben", analog7GruenOben);
        updateIntValue(prefix + "GelbUnten", analog7GelbUnten);
        updateIntValue(prefix + "GelbOben", analog7GelbOben);
        break;
    #endif
    #if MODUL_ANALOG8
      case 8:
        updateStringValue(prefix + "Name", analog8Name);
        updateBoolValue(prefix + "Webhook", analog8Webhook);
        updateIntValue(prefix + "Minimum", analog8Minimum);
        updateIntValue(prefix + "Maximum", analog8Maximum);
        updateIntValue(prefix + "GruenUnten", analog8GruenUnten);
        updateIntValue(prefix + "GruenOben", analog8GruenOben);
        updateIntValue(prefix + "GelbUnten", analog8GelbUnten);
        updateIntValue(prefix + "GelbOben", analog8GelbOben);
        break;
    #endif
  }
}

void updateIntValue(const String& argName, int& value) {
  if (Webserver.arg(argName) != "") {
    value = Webserver.arg(argName).toInt();
  }
}

void updateStringValue(const String& argName, String& value) {
  if (Webserver.arg(argName) != "") {
    value = Webserver.arg(argName);
  }
}

void updateBoolValue(const String& argName, bool& value) {
  if (Webserver.hasArg(argName)) {
    value = true;
  } else {
    value = false;
  }
}

void sendLinks() {
  Webserver.sendContent(F(
    "<div class=\"weiss\">\n"
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
}
