/* Funktion: ArgumenteAusgeben()
 * Gibt alle Argumente aus, die übergeben wurden.
 */
void ArgumenteAusgeben() {
  Serial.println("Gebe alle Argumente des POST requests aus:");
  int numArgs = Webserver.args();
  for (int i = 0; i < numArgs; i++) {
    String argName = Webserver.argName(i);
    String argValue = Webserver.arg(i);
    Serial.print(argName);
    Serial.print(": ");
    Serial.println(argValue);
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
  String formatierterCode = htmlHeader; // formatierterCode beginnt mit Kopf der HTML-Seite
  if ( Webserver.arg("Passwort") == wifiAdminPasswort) { // wenn das Passwort stimmt
    #if MODUL_LEDAMPEL
      if ( Webserver.arg("ampelModus") != "" ) { // wenn ein neuer Wert für ampelModus übergeben wurde
        ampelModus = Webserver.arg("ampelModus").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_DISPLAY
      if ( Webserver.arg("status") != "" ) { // wenn ein neuer Wert für status übergeben wurde
          status = Webserver.arg("status").toInt(); // neuen Wert übernehmen
        }
    #endif
    #if MODUL_DHT
      if ( Webserver.arg("lufttemperaturGruenUnten") != "" ) { // wenn ein neuer Wert für lufttemperaturGruenUnten übergeben wurde
        lufttemperaturGruenUnten = Webserver.arg("lufttemperaturGruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("lufttemperaturGruenOben") != "" ) { // wenn ein neuer Wert für lufttemperaturGruenOben übergeben wurde
        lufttemperaturGruenOben = Webserver.arg("lufttemperaturGruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("lufttemperaturGelbUnten") != "" ) { // wenn ein neuer Wert für lufttemperaturGelbUnten übergeben wurde
        lufttemperaturGelbUnten = Webserver.arg("lufttemperaturGelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("lufttemperaturGelbOben") != "" ) { // wenn ein neuer Wert für lufttemperaturGelbOben übergeben wurde
        lufttemperaturGelbOben = Webserver.arg("lufttemperaturGelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_WEBHOOK
      webhookSchalter = Webserver.arg("webhookSchalter").toInt(); // neuen Wert übernehmen
      if ( Webserver.arg("webhookFrequenz") != "" ) { // wenn ein neuer Wert für lufttemperaturGruenOben übergeben wurde
        webhookFrequenz = Webserver.arg("webhookFrequenz").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("webhookDomain") != "" ) { // wenn ein neuer Wert für lufttemperaturGruenOben übergeben wurde
        webhookFrequenz = Webserver.arg("webhookDomain").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("webhookPfad") != "" ) { // wenn ein neuer Wert für lufttemperaturGruenOben übergeben wurde
        webhookFrequenz = Webserver.arg("webhookPfad").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_HELLIGKEIT
      if ( Webserver.arg("helligkeitName") != "" ) { // wenn ein neuer Wert für helligkeitName übergeben wurde
        helligkeitName = Webserver.arg("helligkeitName"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitMinimum") != "" ) { // wenn ein neuer Wert für helligkeitMinimum übergeben wurde
        helligkeitMinimum = Webserver.arg("helligkeitMinimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitMaximum") != "" ) { // wenn ein neuer Wert für helligkeitMaximum übergeben wurde
        helligkeitMaximum = Webserver.arg("helligkeitMaximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitGruenUnten") != "" ) { // wenn ein neuer Wert für helligkeitGruenUnten übergeben wurde
        helligkeitGruenUnten = Webserver.arg("helligkeitGruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitGruenOben") != "" ) { // wenn ein neuer Wert für helligkeitGruenOben übergeben wurde
        helligkeitGruenOben = Webserver.arg("helligkeitGruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitGelbUnten") != "" ) { // wenn ein neuer Wert für helligkeitGelbUnten übergeben wurde
        helligkeitGelbUnten = Webserver.arg("helligkeitGelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitGelbOben") != "" ) { // wenn ein neuer Wert für helligkeitGelbOben übergeben wurde
        helligkeitGelbOben = Webserver.arg("helligkeitGelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_BODENFEUCHTE
      if ( Webserver.arg("bodenfeuchteName") != "" ) { // wenn ein neuer Wert für bodenfeuchteName übergeben wurde
        bodenfeuchteName = Webserver.arg("bodenfeuchteName"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("bodenfeuchteMinimum") != "" ) { // wenn ein neuer Wert für bodenfeuchteMinimum übergeben wurde
        bodenfeuchteMinimum = Webserver.arg("bodenfeuchteMinimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("bodenfeuchteMaximum") != "" ) { // wenn ein neuer Wert für bodenfeuchteMaximum übergeben wurde
        bodenfeuchteMaximum = Webserver.arg("bodenfeuchteMaximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("bodenfeuchteGruenUnten") != "" ) { // wenn ein neuer Wert für bodenfeuchteGruenUnten übergeben wurde
        bodenfeuchteGruenUnten = Webserver.arg("bodenfeuchteGruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("bodenfeuchteGruenOben") != "" ) { // wenn ein neuer Wert für bodenfeuchteGruenOben übergeben wurde
        bodenfeuchteGruenOben = Webserver.arg("bodenfeuchteGruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("bodenfeuchteGelbUnten") != "" ) { // wenn ein neuer Wert für bodenfeuchteGelbUnten übergeben wurde
        bodenfeuchteGelbUnten = Webserver.arg("bodenfeuchteGelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("bodenfeuchteGelbOben") != "" ) { // wenn ein neuer Wert für bodenfeuchteGelbOben übergeben wurde
        bodenfeuchteGelbOben = Webserver.arg("bodenfeuchteGelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_ANALOG3
      if ( Webserver.arg("analog3Name") != "" ) { // wenn ein neuer Wert für analog3Name übergeben wurde
        analog3Name = Webserver.arg("analog3Name"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog3Minimum") != "" ) { // wenn ein neuer Wert für analog3Minimum übergeben wurde
        analog3Minimum = Webserver.arg("analog3Minimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog3Maximum") != "" ) { // wenn ein neuer Wert für analog3Maximum übergeben wurde
        analog3Maximum = Webserver.arg("analog3Maximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog3GruenUnten") != "" ) { // wenn ein neuer Wert für analog3GruenUnten übergeben wurde
        analog3GruenUnten = Webserver.arg("analog3GruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog3GruenOben") != "" ) { // wenn ein neuer Wert für analog3GruenOben übergeben wurde
        analog3GruenOben = Webserver.arg("analog3GruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog3GelbUnten") != "" ) { // wenn ein neuer Wert für analog3GelbUnten übergeben wurde
        analog3GelbUnten = Webserver.arg("analog3GelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog3GelbOben") != "" ) { // wenn ein neuer Wert für analog3GelbOben übergeben wurde
        analog3GelbOben = Webserver.arg("analog3GelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_ANALOG4
      if ( Webserver.arg("analog4Name") != "" ) { // wenn ein neuer Wert für analog4Name übergeben wurde
        analog4Name = Webserver.arg("analog4Name"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog4Minimum") != "" ) { // wenn ein neuer Wert für analog4Minimum übergeben wurde
        analog4Minimum = Webserver.arg("analog4Minimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog4Maximum") != "" ) { // wenn ein neuer Wert für analog4Maximum übergeben wurde
        analog4Maximum = Webserver.arg("analog4Maximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog4GruenUnten") != "" ) { // wenn ein neuer Wert für analog4GruenUnten übergeben wurde
        analog4GruenUnten = Webserver.arg("analog4GruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog4GruenOben") != "" ) { // wenn ein neuer Wert für analog4GruenOben übergeben wurde
        analog4GruenOben = Webserver.arg("analog4GruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog4GelbUnten") != "" ) { // wenn ein neuer Wert für analog4GelbUnten übergeben wurde
        analog4GelbUnten = Webserver.arg("analog4GelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog4GelbOben") != "" ) { // wenn ein neuer Wert für analog4GelbOben übergeben wurde
        analog4GelbOben = Webserver.arg("analog4GelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_ANALOG5
      if ( Webserver.arg("analog5Name") != "" ) { // wenn ein neuer Wert für analog5Name übergeben wurde
        analog5Name = Webserver.arg("analog5Name"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog5Minimum") != "" ) { // wenn ein neuer Wert für analog5Minimum übergeben wurde
        analog5Minimum = Webserver.arg("analog5Minimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog5Maximum") != "" ) { // wenn ein neuer Wert für analog5Maximum übergeben wurde
        analog5Maximum = Webserver.arg("analog5Maximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog5GruenUnten") != "" ) { // wenn ein neuer Wert für analog5GruenUnten übergeben wurde
        analog5GruenUnten = Webserver.arg("analog5GruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog5GruenOben") != "" ) { // wenn ein neuer Wert für analog5GruenOben übergeben wurde
        analog5GruenOben = Webserver.arg("analog5GruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog5GelbUnten") != "" ) { // wenn ein neuer Wert für analog5GelbUnten übergeben wurde
        analog5GelbUnten = Webserver.arg("analog5GelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog5GelbOben") != "" ) { // wenn ein neuer Wert für analog5GelbOben übergeben wurde
        analog5GelbOben = Webserver.arg("analog5GelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_ANALOG6
      if ( Webserver.arg("analog6Name") != "" ) { // wenn ein neuer Wert für analog6Name übergeben wurde
        analog6Name = Webserver.arg("analog6Name"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog6Minimum") != "" ) { // wenn ein neuer Wert für analog6Minimum übergeben wurde
        analog6Minimum = Webserver.arg("analog6Minimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog6Maximum") != "" ) { // wenn ein neuer Wert für analog6Maximum übergeben wurde
        analog6Maximum = Webserver.arg("analog6Maximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog6GruenUnten") != "" ) { // wenn ein neuer Wert für analog6GruenUnten übergeben wurde
        analog6GruenUnten = Webserver.arg("analog6GruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog6GruenOben") != "" ) { // wenn ein neuer Wert für analog6GruenOben übergeben wurde
        analog6GruenOben = Webserver.arg("analog6GruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog6GelbUnten") != "" ) { // wenn ein neuer Wert für analog6GelbUnten übergeben wurde
        analog6GelbUnten = Webserver.arg("analog6GelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog6GelbOben") != "" ) { // wenn ein neuer Wert für analog6GelbOben übergeben wurde
        analog6GelbOben = Webserver.arg("analog6GelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_ANALOG7
      if ( Webserver.arg("analog7Name") != "" ) { // wenn ein neuer Wert für analog7Name übergeben wurde
        analog7Name = Webserver.arg("analog7Name"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog7Minimum") != "" ) { // wenn ein neuer Wert für analog7Minimum übergeben wurde
        analog7Minimum = Webserver.arg("analog7Minimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog7Maximum") != "" ) { // wenn ein neuer Wert für analog7Maximum übergeben wurde
        analog7Maximum = Webserver.arg("analog7Maximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog7GruenUnten") != "" ) { // wenn ein neuer Wert für analog7GruenUnten übergeben wurde
        analog7GruenUnten = Webserver.arg("analog7GruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog7GruenOben") != "" ) { // wenn ein neuer Wert für analog7GruenOben übergeben wurde
        analog7GruenOben = Webserver.arg("analog7GruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog7GelbUnten") != "" ) { // wenn ein neuer Wert für analog7GelbUnten übergeben wurde
        analog7GelbUnten = Webserver.arg("analog7GelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog7GelbOben") != "" ) { // wenn ein neuer Wert für analog7GelbOben übergeben wurde
        analog7GelbOben = Webserver.arg("analog7GelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_ANALOG8
      if ( Webserver.arg("analog8Name") != "" ) { // wenn ein neuer Wert für analog8Name übergeben wurde
        analog8Name = Webserver.arg("analog8Name"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog8Minimum") != "" ) { // wenn ein neuer Wert für analog8Minimum übergeben wurde
        analog8Minimum = Webserver.arg("analog8Minimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog8Maximum") != "" ) { // wenn ein neuer Wert für analog8Maximum übergeben wurde
        analog8Maximum = Webserver.arg("analog8Maximum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog8GruenUnten") != "" ) { // wenn ein neuer Wert für analog8GruenUnten übergeben wurde
        analog8GruenUnten = Webserver.arg("analog8GruenUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog8GruenOben") != "" ) { // wenn ein neuer Wert für analog8GruenOben übergeben wurde
        analog8GruenOben = Webserver.arg("analog8GruenOben").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog8GelbUnten") != "" ) { // wenn ein neuer Wert für analog8GelbUnten übergeben wurde
        analog8GelbUnten = Webserver.arg("analog8GelbUnten").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("analog8GelbOben") != "" ) { // wenn ein neuer Wert für analog8GelbOben übergeben wurde
        analog8GelbOben = Webserver.arg("analog8GelbOben").toInt(); // neuen Wert übernehmen
      }
    #endif
    formatierterCode += "<h2>Erfolgreich!</h2>\n";
  } else { // wenn das Passwort falsch ist
    formatierterCode += "<h2>Falsches Passwort!</h2>\n";
  }
  if ( Webserver.arg("loeschen") == "Ja!" ) {
    formatierterCode += "<div class=\"rot\">\n<p>Alle Variablen wurden gelöscht.</p>\n";
    formatierterCode += "<p>Der Pflanzensensor wird neu gestartet.</p>\n</div>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<p><a href=\"/\">Warte ein paar Sekunden, dann kannst du hier zur Startseite zurück.</a></p>\n";
    formatierterCode += "</div>\n";
  } else {
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li><a href=\"/\">zur Startseite</a></li>\n";
    formatierterCode += "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n";
    #if MODUL_DEBUG
    formatierterCode += "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n";
    #endif
    formatierterCode += "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">";
    formatierterCode += "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n";
    formatierterCode += "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">";
    formatierterCode += "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
  }
  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
  if ( Webserver.arg("loeschen") == "Ja!" ) {
    VariablenLoeschen(); // Variablen löschen
    ESP.restart(); // ESP neu starten
  } else {
    VariablenSpeichern(); // Variablen in den Flash speichern
  }
}
