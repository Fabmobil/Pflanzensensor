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
  #include "wifi_bilder.h" // Bilder die auf der Seite verwendet werden
  #include "wifi_header.h" // Kopf der HTML-Seite
  #include "wifi_footer.h" // Fuß der HTML-Seite
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
    #if MODUL_HELLIGKEIT
      if ( Webserver.arg("ampelHelligkeitGruen") != "" ) { // wenn ein neuer Wert für ampelHelligkeitGruen übergeben wurde
        ampelHelligkeitGruen = Webserver.arg("ampelHelligkeitGruen").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("ampelHelligkeitRot") != "" ) { // wenn ein neuer Wert für ampelHelligkeitRot übergeben wurde
        ampelHelligkeitRot = Webserver.arg("ampelHelligkeitRot").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_BODENFEUCHTE
      if ( Webserver.arg("bodenfeuchteName") != "" ) { // wenn ein neuer Wert für bodenfeuchteName übergeben wurde
        bodenfeuchteName = Webserver.arg("bodenfeuchteName"); // neuen Wert übernehmen
      }
      if ( Webserver.arg("ampelBodenfeuchteGruen") != "" ) { // wenn ein neuer Wert für ampelBodenfeuchteGruen übergeben wurde
        ampelBodenfeuchteGruen = Webserver.arg("ampelBodenfeuchteGruen").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("ampelBodenfeuchteRot") != "" ) { // wenn ein neuer Wert für ampelBodenfeuchteRot übergeben wurde
        ampelBodenfeuchteRot = Webserver.arg("ampelBodenfeuchteRot").toInt(); // neuen Wert übernehmen
      }
    #endif
    #if MODUL_DISPLAY
      if ( Webserver.arg("status") != "" ) { // wenn ein neuer Wert für status übergeben wurde
          status = Webserver.arg("status").toInt(); // neuen Wert übernehmen
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
    #endif
    formatierterCode += "<h2>Erfolgreich!</h2>";
  } else { // wenn das Passwort falsch ist
    formatierterCode += "<h2>Falsches Passwort!</h2>";
  }
  formatierterCode += "<ul>";
  formatierterCode += "<li><a href=\"/\">zur Startseite</a></li>";
  formatierterCode += "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>";
  #if MODUL_DEBUG
  formatierterCode += "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>";
  #endif
  formatierterCode += "<li><a href=\"https://www.github.com/pippcat/Pflanzensensor\" target=\"_blank\"><img src=\"";
  formatierterCode += logoGithub;
  formatierterCode += "\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>";
  formatierterCode += "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\"><img src=\"";
  formatierterCode += logoFabmobil;
  formatierterCode += "\">&nbspHomepage</a></li>";
  formatierterCode += "</ul>";
  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
}
