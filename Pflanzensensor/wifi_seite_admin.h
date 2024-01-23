/* Funktion: String GeneriereAnalogsensorAdminString(int sensorNummer, const String& sensorName, const int minimum, const int maximum)
 * Generiert einen String für einen Analogsensor
 * Parameter:
 * int sensorNummer: Nummer des Sensors
 * String sensorName: Name des Sensors
 * int minimum: Minimalwert des Sensors
 * int maximum: Maximalwert des Sensors
 * Rückgabewert: formatierter String
 */
String GeneriereAnalogsensorAdminString(
int sensorNummer,
const String& sensorName,
const int minimum,
const int maximum,
const int gruenUnten,
const int gruenOben,
const int gelbUnten,
const int gelbOben) {
  String analogsensorAdminString;
  analogsensorAdminString += "<h2>Analogsensor " + String(sensorNummer) + "</h2>\n";
  analogsensorAdminString += "<div class=\"weiss\">\n<p>Sensorname: ";
  analogsensorAdminString += "<input type=\"text\" size=\"20\" name=\"analog" + String(sensorNummer) + "Name\" placeholder=\"" + String(sensorName) + "\"></p>\n";
  analogsensorAdminString += "<p>Minimalwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "Minimum\" placeholder=\"" + String(minimum) + "\"></p>\n";
  analogsensorAdminString += "<p>Maximalwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "Maximum\" placeholder=\"" + String(maximum) + "\"></p>\n";
  analogsensorAdminString += "<p>unterer grüner Schwellwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "GruenUnten\" placeholder=\"" + String(gruenUnten) + "\"></p>\n";
  analogsensorAdminString += "<p>oberer grüner Schwellwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "GruenOben\" placeholder=\"" + String(gruenOben) + "\"></p>\n";
  analogsensorAdminString += "<p>unterer gelber Schwellwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "GelbUnten\" placeholder=\"" + String(gelbUnten) + "\"></p>\n";
  analogsensorAdminString += "<p>oberer gelber Schwellwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "GelbOben\" placeholder=\"" + String(gelbOben) + "\"></p>\n</div>\n";
  return analogsensorAdminString;
}

/*
 * Funktion: Void WebseiteAdminAusgeben()
 * Gibt die Administrationsseite des Webservers aus.
 */
void WebseiteAdminAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebsiteAdminAusgeben()"));
  #endif
  #include "wifi_header_2.h"
  #include "wifi_footer.h"
  String formatierterCode = htmlHeader;
  formatierterCode += "<h1>Adminseite</h1>\n";
  formatierterCode += "<div class=\"weiss\"><p>Auf dieser Seite können die Variablen verändert werden.</p>\n";
  formatierterCode += "<p>Die Felder zeigen in grau die derzeit gesetzten Werte an. Falls kein neuer Wert eingegeben wird, bleibt der alte Wert erhalten.</p>\n</div>\n";
  formatierterCode += "<form action=\"/setzeVariablen\" method=\"POST\">\n";
  #if MODUL_BODENFEUCHTE
    formatierterCode += "<h2>Bodenfeuchte</h2>\n";
    formatierterCode += "<div class=\"weiss\">\n<p>Sensorname: ";
    formatierterCode += "<input type=\"text\" size=\"20\" name=\"bodenfeuchteName\" placeholder=\"";
    formatierterCode += bodenfeuchteName;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>Minimalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteMinimum\" placeholder=\"";
    formatierterCode += bodenfeuchteMinimum;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>Maximalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteMaximum\" placeholder=\"";
    formatierterCode += bodenfeuchteMaximum;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>unterer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteGruenUnten\" placeholder=\"";
    formatierterCode += bodenfeuchteGruenUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteGruenOben\" placeholder=\"";
    formatierterCode += bodenfeuchteGruenOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>unterer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteGelbUnten\" placeholder=\"";
    formatierterCode += bodenfeuchteGelbUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteGelbOben\" placeholder=\"";
    formatierterCode += bodenfeuchteGelbOben;
    formatierterCode += "\"></p>\n</div>\n";
  #endif
  #if MODUL_DHT
    formatierterCode += "<h2>DHT Modul</h2>\n";
    formatierterCode += "<h3>Lufttemperatur</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<p>unterer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"lufttemperaturGruenUnten\" placeholder=\"";
    formatierterCode += lufttemperaturGruenUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"lufttemperaturGruenOben\" placeholder=\"";
    formatierterCode += lufttemperaturGruenOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>unterer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"lufttemperaturGelbUnten\" placeholder=\"";
    formatierterCode += lufttemperaturGelbUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"lufttemperaturGelbOben\" placeholder=\"";
    formatierterCode += lufttemperaturGelbOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "</div>\n";
    formatierterCode += "<h3>Luftfeuchte</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<p>unterer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"luftfeuchteGruenUnten\" placeholder=\"";
    formatierterCode += luftfeuchteGruenUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"luftfeuchteGruenOben\" placeholder=\"";
    formatierterCode += luftfeuchteGruenOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>unterer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"luftfeuchteGelbUnten\" placeholder=\"";
    formatierterCode += luftfeuchteGelbUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"luftfeuchteGelbOben\" placeholder=\"";
    formatierterCode += luftfeuchteGelbOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "</div>\n";
  #endif

  #if MODUL_HELLIGKEIT
    formatierterCode += "<h2>Helligkeitssensor</h2>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<p>Sensorname: ";
    formatierterCode += "<input type=\"text\" size=\"20\" name=\"helligkeitName\" placeholder=\"";
    formatierterCode += helligkeitName;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>Minimalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitMinimum\" placeholder=\"";
    formatierterCode += helligkeitMinimum;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>Maximalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitMaximum\" placeholder=\"";
    formatierterCode += helligkeitMaximum;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>unterer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitGruenUnten\" placeholder=\"";
    formatierterCode += helligkeitGruenUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer grüner Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitGruenOben\" placeholder=\"";
    formatierterCode += helligkeitGruenOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>unterer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitGelbUnten\" placeholder=\"";
    formatierterCode += helligkeitGelbUnten;
    formatierterCode += "\"></p>\n";
    formatierterCode += "<p>oberer gelber Schwellwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitGelbOben\" placeholder=\"";
    formatierterCode += helligkeitGelbOben;
    formatierterCode += "\"></p>\n";
    formatierterCode += "</div>\n";
  #endif
  #if MODUL_LEDAMPEL
    formatierterCode += "<h2>LED Ampel</h2>\n";
    formatierterCode += "<h3>Anzeigemodus</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<p>Modus: (0: Helligkeit und Bodenfeuchte; 1: Helligkeit; 2: Bodenfeuchte): ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"ampelModus\" placeholder=\"";
    formatierterCode += ampelModus;
    formatierterCode += "\"></p>\n";
    formatierterCode += "</div>\n";
  #endif
  #if MODUL_ANALOG3
    formatierterCode += GeneriereAnalogsensorAdminString(3, analog3Name, analog3Minimum, analog3Maximum, analog3GruenUnten, analog3GruenOben, analog3GelbUnten, analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    formatierterCode += GeneriereAnalogsensorAdminString(4, analog4Name, analog4Minimum, analog4Maximum, analog4GruenUnten, analog4GruenOben, analog4GelbUnten, analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    formatierterCode += GeneriereAnalogsensorAdminString(5, analog5Name, analog5Minimum, analog5Maximum, analog5GruenUnten, analog5GruenOben, analog5GelbUnten, analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    formatierterCode += GeneriereAnalogsensorAdminString(6, analog6Name, analog6Minimum, analog6Maximum, analog6GruenUnten, analog6GruenOben, analog6GelbUnten, analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    formatierterCode += GeneriereAnalogsensorAdminString(7, analog7Name, analog7Minimum, analog7Maximum, analog7GruenUnten, analog7GruenOben, analog7GelbUnten, analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    formatierterCode += GeneriereAnalogsensorAdminString(8, analog8Name, analog8Minimum, analog8Maximum, analog8GruenUnten, analog8GruenOben, analog8GelbUnten, analog8GelbOben);
  #endif

  formatierterCode += "<h2>Einstellungen löschen?</h2>\n";
  formatierterCode += "<div class=\"rot\">\n<p>";
  formatierterCode += "GEFAHR: Wenn du hier \"Ja!\" eingibst, werden alle Einstellungen gelöscht und die Werte, ";
  formatierterCode += "die beim Flashen eingetragen wurden, werden wieder gesetzt. Der Pflanzensensor startet neu.";
  formatierterCode += "</p>\n<p><input type=\"text\" size=\"4\" name=\"loeschen\" placeholder=\"nein\"></p>\n</div>\n";
  formatierterCode += "<h2>Passwort</h2>\n";
  formatierterCode += "<div class=\"weiss\">";
  formatierterCode += "<p><input type=\"password\" name=\"Passwort\" placeholder=\"Passwort\"><br>";
  formatierterCode += "<input type=\"submit\" value=\"Absenden\"></p></form>";
  formatierterCode += "</div>\n";

  formatierterCode += "<h2>Links</h2>\n";
  formatierterCode += "<div class=\"weiss\">\n";
  formatierterCode += "<ul>\n";
  formatierterCode += "<li><a href=\"/\">zur Startseite</a></li>\n";
  #if MODUL_DEBUG
  formatierterCode += "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n";
  #endif
  formatierterCode += "<li><a href=\"https://www.github.com/pippcat/Pflanzensensor\" target=\"_blank\">";
  formatierterCode += "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n";
  formatierterCode += "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">";
  formatierterCode += "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n";
  formatierterCode += "</ul>\n";
  formatierterCode += "</div>\n";

  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
  #if MODUL_DEBUG
    Serial.println(F("# Ende von WebsiteAdminAusgeben()"));
  #endif
}
