/* Funktion: String GeneriereAnalogsensorAdminString(int sensorNummer, const String& sensorName, const int minimum, const int maximum)
 * Generiert einen String für einen Analogsensor
 * Parameter:
 * int sensorNummer: Nummer des Sensors
 * String sensorName: Name des Sensors
 * int minimum: Minimalwert des Sensors
 * int maximum: Maximalwert des Sensors
 * Rückgabewert: formatierter String
 */
String GeneriereAnalogsensorAdminString(int sensorNummer, const String& sensorName, const int minimum, const int maximum) {
  String analogsensorAdminString;
  analogsensorAdminString += "<h2>Analogsensor " + String(sensorNummer) + "</h2>";
  analogsensorAdminString += "<p>Sensorname: ";
  analogsensorAdminString += "<input type=\"text\" size=\"20\" name=\"analog" + String(sensorNummer) + "Name\" placeholder=\"" + String(sensorName) + "\"></p>";
  analogsensorAdminString += "<p>Minimalwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "Minimum\" placeholder=\"" + String(minimum) + "\"></p>";
  analogsensorAdminString += "<p>Maximalwert: ";
  analogsensorAdminString += "<input type=\"text\" size=\"4\" name=\"analog" + String(sensorNummer) + "Maximum\" placeholder=\"" + String(maximum) + "\"></p>";
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
  #include "wifi_bilder.h"
  #include "wifi_header.h"
  #include "wifi_footer.h"
  String formatierterCode = htmlHeader;
  formatierterCode += "<h1>Adminseite</h1>";
  formatierterCode += "<p>Auf dieser Seite können die Variablen verändert werden.</p>";
  formatierterCode += "<p>Die Felder zeigen in grau die derzeit gesetzten Werte an. Falls kein neuer Wert eingegeben wird, bleibt der alte Wert erhalten.</p>";
  formatierterCode += "<form action=\"/setzeVariablen\" method=\"POST\">";
  #if MODUL_BODENFEUCHTE
    formatierterCode += "<h2>Bodenfeuchte</h2>";
    formatierterCode += "<p>Sensorname: ";
    formatierterCode += "<input type=\"text\" size=\"20\" name=\"bodenfeuchteName\" placeholder=\"";
    formatierterCode += bodenfeuchteName;
    formatierterCode += "\"></p>";
    formatierterCode += "<p>Minimalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteMinimum\" placeholder=\"";
    formatierterCode += bodenfeuchteMinimum;
    formatierterCode += "\"></p>";
    formatierterCode += "<p>Maximalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"bodenfeuchteMaximum\" placeholder=\"";
    formatierterCode += bodenfeuchteMaximum;
    formatierterCode += "\"></p>";
  #endif
  #if MODUL_HELLIGKEIT
    formatierterCode += "<h2>Helligkeitssensor</h2>";
    formatierterCode += "<p>Sensorname: ";
    formatierterCode += "<input type=\"text\" size=\"20\" name=\"helligkeitName\" placeholder=\"";
    formatierterCode += helligkeitName;
    formatierterCode += "\"></p>";
    formatierterCode += "<p>Minimalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitMinimum\" placeholder=\"";
    formatierterCode += helligkeitMinimum;
    formatierterCode += "\"></p>";
    formatierterCode += "<p>Maximalwert: ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"helligkeitMaximum\" placeholder=\"";
    formatierterCode += helligkeitMaximum;
    formatierterCode += "\"></p>";
  #endif
  #if MODUL_LEDAMPEL
    formatierterCode += "<h2>LED Ampel</h2>";
    formatierterCode += "<h3>Anzeigemodus</h3>";
    formatierterCode += "<p>Modus: (0: Helligkeit und Bodenfeuchte; 1: Helligkeit; 2: Bodenfeuchte): ";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"ampelModus\" placeholder=\"";
    formatierterCode += ampelModus;
    formatierterCode += "\"></p>";
    #if MODUL_HELLIGKEIT
      formatierterCode += "<h3>Helligkeitsanzeige</h3>";
      formatierterCode += "<p>";
      if ( ampelHelligkeitInvertiert ) {
        formatierterCode += "<input type=\"radio\" name=\"ampelHelligkeitInvertiert\" value=\"true\" checked> Skale invertiert<br>";
        formatierterCode += "<input type=\"radio\" name=\"ampelHelligkeitInvertiert\" value=\"false\"> Skale nicht invertiert";
      } else {
        formatierterCode += "<input type=\"radio\" name=\"ampelHelligkeitInvertiert\" value=\"true\"> Skale invertiert<br>";
        formatierterCode += "<input type=\"radio\" name=\"ampelHelligkeitInvertiert\" value=\"false\" checked> Skale nicht invertiert";
      }
      formatierterCode += "</p>";
      formatierterCode += "<p>Schwellwert gruen: ";
      formatierterCode += "<input type=\"text\" size=\"4\" name=\"ampelHelligkeitGruen\" placeholder=\"";
      formatierterCode += ampelHelligkeitGruen;
      formatierterCode += "\"></p>";
      formatierterCode += "<p>Schwellwert rot: ";
      formatierterCode += "<input type=\"text\" size=\"4\" name=\"ampelHelligkeitRot\" placeholder=\"";
      formatierterCode += ampelHelligkeitRot;
      formatierterCode += "\"></p>";
    #endif
    #if MODUL_BODENFEUCHTE
      formatierterCode += "<h3>Bodenfeuchteanzeige</h3>";
      formatierterCode += "<p>";
      if ( ampelBodenfeuchteInvertiert ) {
        formatierterCode += "<input type=\"radio\" name=\"ampelBodenfeuchteInvertiert\" value=\"true\" checked> Skale invertiert<br>";
        formatierterCode += "<input type=\"radio\" name=\"ampelBodenfeuchteInvertiert\" value=\"false\"> Skale nicht invertiert";
      } else {
        formatierterCode += "<input type=\"radio\" name=\"ampelBodenfeuchteInvertiert\" value=\"true\"> Skale invertiert<br>";
        formatierterCode += "<input type=\"radio\" name=\"ampelBodenfeuchteInvertiert\" value=\"false\" checked> Skale nicht invertiert";
      }
      formatierterCode += "</p>";
      formatierterCode += "<p>Schwellwert gruen: ";
      formatierterCode += "<input type=\"text\" size=\"4\" name=\"ampelBodenfeuchteGruen\" placeholder=\"";
      formatierterCode += ampelBodenfeuchteGruen;
      formatierterCode += "\"></p>";
      formatierterCode += "<p>Schwellwert rot: ";
      formatierterCode += "<input type=\"text\" size=\"4\" name=\"ampelBodenfeuchteRot\" placeholder=\"";
      formatierterCode += ampelBodenfeuchteRot;
      formatierterCode += "\"></p>";
    #endif
  #endif
  #if MODUL_ANALOG3
    formatierterCode += GeneriereAnalogsensorAdminString(3, analog3Name, analog3Minimum, analog3Maximum);
  #endif
  #if MODUL_ANALOG4
    formatierterCode += GeneriereAnalogsensorAdminString(4, analog4Name, analog4Minimum, analog4Maximum);
  #endif
  #if MODUL_ANALOG5
    formatierterCode += GeneriereAnalogsensorAdminString(5, analog5Name, analog5Minimum, analog5Maximum);
  #endif
  #if MODUL_ANALOG6
    formatierterCode += GeneriereAnalogsensorAdminString(6, analog6Name, analog6Minimum, analog6Maximum);
  #endif
  #if MODUL_ANALOG7
    formatierterCode += GeneriereAnalogsensorAdminString(7, analog7Name, analog7Minimum, analog7Maximum);
  #endif
  #if MODUL_ANALOG8
    formatierterCode += GeneriereAnalogsensorAdminString(8, analog8Name, analog8Minimum, analog8Maximum);
  #endif

  formatierterCode += "<h2>Passwort</h2>";
  formatierterCode += "<p><input type=\"password\" name=\"Passwort\" placeholder=\"Passwort\"><br>";
  formatierterCode += "<input type=\"submit\" value=\"Absenden\"></p></form>";

  formatierterCode += "<h2>Links</h2>";
  formatierterCode += "<ul>";
  formatierterCode += "<li><a href=\"/\">zur Startseite</a></li>";
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
