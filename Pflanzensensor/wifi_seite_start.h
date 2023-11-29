/* Funktion: String GeneriereSensorString(int sensorNummer, const String& sensorName, const String& messwert, const String& einheit)
 * Generiert einen String für einen Sensor
 * int sensorNummer: Nummer des Sensors
 * String sensorName: Name des Sensors
 * int messwert: Messwert des Sensors
 * String einheit: Einheit des Messwerts
 * Rückgabewert: formatierter String
 */
String GeneriereSensorString(const int sensorNummer, const String& sensorName, const int messwert, const String& einheit) {
  String sensorString;
  if (sensorNummer == 0) {
    sensorString += "<h2>" + sensorName + "</h2><p>" + messwert + " " + einheit + "</p>";
    return sensorString;
  } else {
    sensorString += "<h2>Analogsensor " + String(sensorNummer) + ": " + sensorName + "</h2>";
    sensorString += "<p>" + String(messwert) + " " + einheit + "</p>";
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
  #include "wifi_bilder.h" // Bilder die auf der Seite verwendet werden
  #include "wifi_header.h" // Kopf der HTML-Seite
  #include "wifi_footer.h" // Fuß der HTML-Seite
  String formatierterCode = htmlHeader;
  formatierterCode += "<p>Diese Seite zeigt die Sensordaten deines Pflanzensensors an. Sie aktualisiert sich automatisch aller 10 Sekunden.</p>";
  #if MODUL_HELLIGKEIT
    formatierterCode += GeneriereSensorString(0, helligkeitName, messwertHelligkeitProzent, "%");
  #endif
  #if MODUL_BODENFEUCHTE
    formatierterCode += GeneriereSensorString(0, bodenfeuchteName, messwertBodenfeuchteProzent, "%");
  #endif
  #if MODUL_DHT
    formatierterCode += GeneriereSensorString(0, "Lufttemperatur", messwertLufttemperatur, "°C");
    formatierterCode += GeneriereSensorString(0, "Luftfeuchte", messwertLuftfeuchte, "%");
  #endif
  #if MODUL_ANALOG3
    formatierterCode += GeneriereSensorString(3, analog3Name, messwertAnalog3Prozent, "%");
  #endif
  #if MODUL_ANALOG4
    formatierterCode += GeneriereSensorString(4, analog4Name, messwertAnalog4Prozent, "%");
  #endif
  #if MODUL_ANALOG5
    formatierterCode += GeneriereSensorString(5, analog5Name, messwertAnalog5Prozent, "%");
  #endif
  #if MODUL_ANALOG6
    formatierterCode += GeneriereSensorString(6, analog6Name, messwertAnalog6Prozent, "%");
  #endif
  #if MODUL_ANALOG7
    formatierterCode += GeneriereSensorString(7, analog7Name, messwertAnalog7Prozent, "%");
  #endif
  #if MODUL_ANALOG8
    formatierterCode += GeneriereSensorString(8, analog8Name, messwertAnalog8Prozent, "%");
  #endif
  formatierterCode += "<h2>Links</h2>";
  formatierterCode += "<ul>";
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
