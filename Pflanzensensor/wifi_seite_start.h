/* Funktion: String GeneriereSensorString(int sensorNummer, const String& sensorName, const String& messwert, const String& einheit)
 * Generiert einen String für einen Sensor
 * int sensorNummer: Nummer des Sensors
 * String sensorName: Name des Sensors
 * String sensorFarbe: Farbe des Sensors
 * int messwert: Messwert des Sensors
 * String einheit: Einheit des Messwerts
 * Rückgabewert: formatierter String
 */
String GeneriereSensorString(const int sensorNummer, const String& sensorName, const String& sensorFarbe,
const int messwert, const String& einheit) {
  String sensorString;
  if (sensorNummer == 0) {
    sensorString += "<h2>" + sensorName + "</h2><div class=\"" + sensorFarbe + "\"><p>" + messwert + " " + einheit + "</p></div>";
    return sensorString;
  } else {
    sensorString += "<h2>Analogsensor " + String(sensorNummer) + ": " + sensorName + "</h2>";
    sensorString += "<div class=\"" + sensorFarbe + "\"><p>" + String(messwert) + " " + einheit + "</p></div>";
    return sensorString;
  }
}


/*
 * Funktion: Void WebseiteStartAusgeben()
 * Gibt die Startseite des Webservers aus.
 */
void WebseiteStartAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebsiteStartAusgeben()"));
  #endif
  #include "wifi_header.h" // Kopf der HTML-Seite
  #include "wifi_footer.h" // Fuß der HTML-Seite
  String formatierterCode = htmlHeader;
  formatierterCode += "<div class=\"weiss\">";
  formatierterCode += "<p>Diese Seite zeigt die Sensordaten deines Pflanzensensors an. Sie aktualisiert sich automatisch aller 10 Sekunden.</p>";
  formatierterCode += "</div>";
  #if MODUL_HELLIGKEIT
    formatierterCode += GeneriereSensorString(0, helligkeitName, helligkeitFarbe, helligkeitMesswertProzent, "%");
  #endif
  #if MODUL_BODENFEUCHTE
    formatierterCode += GeneriereSensorString(0, bodenfeuchteName, bodenfeuchteFarbe, bodenfeuchteMesswertProzent, "%");
  #endif
  #if MODUL_DHT
    formatierterCode += GeneriereSensorString(0, "Lufttemperatur", lufttemperaturFarbe, lufttemperaturMesswert, "°C");
    formatierterCode += GeneriereSensorString(0, "Luftfeuchte", luftfeuchteFarbe, luftfeuchteMesswert, "%");
  #endif
  #if MODUL_ANALOG3
    formatierterCode += GeneriereSensorString(3, analog3Name, analog3Farbe, analog3MesswertProzent, "%");
  #endif
  #if MODUL_ANALOG4
    formatierterCode += GeneriereSensorString(4, analog4Name, analog4Farbe, analog4MesswertProzent, "%");
  #endif
  #if MODUL_ANALOG5
    formatierterCode += GeneriereSensorString(5, analog5Name, analog5Farbe, analog5MesswertProzent, "%");
  #endif
  #if MODUL_ANALOG6
    formatierterCode += GeneriereSensorString(6, analog6Name, analog6Farbe, analog6MesswertProzent, "%");
  #endif
  #if MODUL_ANALOG7
    formatierterCode += GeneriereSensorString(7, analog7Name, analog7Farbe, analog7MesswertProzent, "%");
  #endif
  #if MODUL_ANALOG8
    formatierterCode += GeneriereSensorString(8, analog8Name, analog8Farbe, analog8MesswertProzent, "%");
  #endif
  formatierterCode += "<h2>Links</h2>";
  formatierterCode += "<div class=\"weiss\">";
  formatierterCode += "<ul>";
  formatierterCode += "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>";
  #if MODUL_DEBUG
  formatierterCode += "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>";
  #endif
  formatierterCode += "<li><a href=\"https://www.github.com/pippcat/Pflanzensensor\" target=\"_blank\">";
  formatierterCode += "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>";
  formatierterCode += "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">";
  formatierterCode += "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>";
  formatierterCode += "</ul>";
  formatierterCode += "</div>";
  formatierterCode += htmlFooter;
  Webserver.send(200, "text/html", formatierterCode);
}
