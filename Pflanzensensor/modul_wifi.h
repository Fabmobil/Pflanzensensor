/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul und den Webserver
 */

#include <ESP8266WiFi.h> // für WLAN
#include <ESP8266WebServer.h> // für Webserver
#include <ESP8266mDNS.h> // für Namensauflösung

WiFiClient client;
ESP8266WebServer Webserver(80); // Webserver auf Port 80

/* Funktion: String GeneriereSensorString(int sensorNummer, const String& sensorName, const String& messwert, const String& einheit)
 * Generiert einen String für einen Sensor
 * Parameter:
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

/* Funktion: String GeneriereAnalogsensorDebugString(int sensorNummer, const String& sensorName, const int messwert, const int messwertProzent, const int minimum, const int maximum)
 * Generiert einen String für einen Analogsensor
 * Parameter:
 * int sensorNummer: Nummer des Sensors
 * String sensorName: Name des Sensors
 * int messwert: Messwert des Sensors
 * int messwertProzent: Messwert des Sensors in Prozent
 * int minimum: Minimalwert des Sensors
 * int maximum: Maximalwert des Sensors
 * Rückgabewert: formatierter String
 */
String GeneriereAnalogsensorDebugString(const int sensorNummer, const String& sensorName, const int messwert,
    const int messwertProzent, const int minimum, const int maximum) {
  String analogsensorDebugString;
  analogsensorDebugString += "<h3>Analogsensor " + String(sensorNummer) + " Modul</h3><ul>";
  analogsensorDebugString += "<li>Sensorname: " + String(sensorName) + "</li>";
  analogsensorDebugString += "<li>Messwert Prozent: " + String(messwertProzent) + "</li>";
  analogsensorDebugString += "<li>Messwert: " + String(messwert) + "</li>";
  analogsensorDebugString += "<li>Minimalwert: " + String(minimum) + "</li>";
  analogsensorDebugString += "<li>Maximalwert: " + String(maximum) + "</li></ul>";
  return analogsensorDebugString;
}

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
 * Funktion: Void WebseiteStartAusgeben()
 * Gibt die Startseite des Webservers aus.
 */
void WebseiteStartAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebsiteStartAusgeben()"));
  #endif
  #include "modul_wifi_bilder.h" // Bilder die auf der Seite verwendet werden
  #include "modul_wifi_header.h" // Kopf der HTML-Seite
  #include "modul_wifi_footer.h" // Fuß der HTML-Seite
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

/*
 * Funktion: Void WebseiteSetzeVariablen()
 * Setzt die Variablen, die über die Adminseite geändert werden können.
 */
void WebseiteDebugAusgeben() {
  #include "modul_wifi_bilder.h"
  #include "modul_wifi_header.h"
  #include "modul_wifi_footer.h"
  String formatierterCode = htmlHeader;
  formatierterCode += "<h2>Debug-Informationen</h2>";
  formatierterCode += "<ul>";
  formatierterCode += "<li>Anzahl Module: ";
  formatierterCode += module;
  formatierterCode += "</li>";
  formatierterCode += "</ul>";

  #if MODUL_DHT
    formatierterCode += "<h3>DHT Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Lufttemperatur: ";
    formatierterCode += messwertLufttemperatur;
    formatierterCode += "</li>";
    formatierterCode += "<li>Luftfeuchte: ";
    formatierterCode += messwertLuftfeuchte;
    formatierterCode += "</li>";
    formatierterCode += "<li>DHT Pin: ";
    formatierterCode += pinDht;
    formatierterCode += "</li>";
    formatierterCode += "<li>DHT Sensortyp: ";
    formatierterCode += dhtSensortyp;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #endif

  #if MODUL_DISPLAY
    formatierterCode += "<h3>Display Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Aktives Displaybild: ";
    formatierterCode += status;
    formatierterCode += "</li>";
    formatierterCode += "<li>Breite in Pixel: ";
    formatierterCode += displayBreite;
    formatierterCode += "</li>";
    formatierterCode += "<li>Hoehe in Pixel: ";
    formatierterCode += displayHoehe;
    formatierterCode += "</li>";
    formatierterCode += "<li>Adresse: ";
    formatierterCode += displayAdresse;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #endif

  #if MODUL_BODENFEUCHTE
    formatierterCode += "<h3>Bodenfeuchte Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += messwertBodenfeuchteProzent;
    formatierterCode += "</li>";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += messwertBodenfeuchte;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
 #endif

  #if MODUL_LEDAMPEL
    formatierterCode += "<h3>LEDAmpel Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Modus: ";
    formatierterCode += ampelModus;
    formatierterCode += "</li>";
    formatierterCode += "<li>ampelUmschalten: ";
    formatierterCode += ampelUmschalten;
    formatierterCode += "</li>";
    formatierterCode += "<li>Pin gruene LED: ";
    formatierterCode += pinAmpelGruen;
    formatierterCode += "</li>";
    formatierterCode += "<li>Pin gelbe LED: ";
    formatierterCode += pinAmpelGelb;
    formatierterCode += "</li>";
    formatierterCode += "<li>Pin rote LED: ";
    formatierterCode += pinAmpelRot;
    formatierterCode += "</li>";
    formatierterCode += "<li>Bodenfeuchte Schwellwert gruen: ";
    formatierterCode += ampelBodenfeuchteGruen;
    formatierterCode += "</li>";
    formatierterCode += "<li>Bodenfeuchte Schwellwert rot: ";
    formatierterCode += ampelBodenfeuchteRot;
    formatierterCode += "</li>";
    formatierterCode += "<li>Bodenfeuchte Skala invertiert?: ";
    formatierterCode += ampelBodenfeuchteInvertiert;
    formatierterCode += "</li>";
    formatierterCode += "<li>Helligkeit Schwellwert gruen: ";
    formatierterCode += ampelHelligkeitGruen;
    formatierterCode += "</li>";
    formatierterCode += "<li>Helligkeit Schwellwert rot: ";
    formatierterCode += ampelHelligkeitRot;
    formatierterCode += "</li>";
    formatierterCode += "<li>Helligkeit Skala invertiert?: ";
    formatierterCode += ampelHelligkeitInvertiert;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #endif

  #if MODUL_HELLIGKEIT
    formatierterCode += "<h3>Helligkeit Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += messwertHelligkeitProzent;
    formatierterCode += "</li>";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += messwertHelligkeit;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #endif

  #if MODUL_WIFI
    formatierterCode += "<h3>Wifi Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Hostname: ";
    formatierterCode += wifiHostname;
    formatierterCode += ".local</li>";
    if ( wifiAp == false ) { // falls der ESP in einem anderen WLAN ist:
      formatierterCode += "<li>SSID: ";
      formatierterCode += wifiSsid;
      formatierterCode += "</li>";
      formatierterCode += "<li>Passwort: ";
      formatierterCode += wifiPassword;
      formatierterCode += "</li>";
    } else { // falls der ESP sein eigenes WLAN aufmacht:
      formatierterCode += "<li>Name des WLANs: ";
      formatierterCode += wifiApSsid;
      formatierterCode += "</li>";
      formatierterCode += "<li>Passwort: ";
      if ( wifiApPasswortAktiviert ) {
        formatierterCode += wifiPassword;
      } else {
        formatierterCode += "WLAN ohne Passwortschutz!";
      }
      formatierterCode += "</li>";
    }
    formatierterCode += "</ul>";
  #endif
  #if MODUL_IFTTT
    formatierterCode += "<h3>IFTTT Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>IFTTT Passwort: ";
    formatierterCode += wifiIftttPasswort;
    formatierterCode += "</li>";
    formatierterCode += "<li>IFTTT Ereignis: ";
    formatierterCode += wifiIftttEreignis;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
 #endif

  #if MODUL_ANALOG3
    formatierterCode += GeneriereAnalogsensorDebugString(3, analog3Name, messwertAnalog3, messwertAnalog3Prozent, analog3Minimum, analog3Maximum);
  #endif
  #if MODUL_ANALOG4
    formatierterCode += GeneriereAnalogsensorDebugString(4, analog4Name, messwertAnalog4, messwertAnalog4Prozent, analog4Minimum, analog4Maximum);
  #endif
  #if MODUL_ANALOG5
    formatierterCode += GeneriereAnalogsensorDebugString(5, analog5Name, messwertAnalog5, messwertAnalog5Prozent, analog5Minimum, analog5Maximum);
  #endif
  #if MODUL_ANALOG6
    formatierterCode += GeneriereAnalogsensorDebugString(6, analog6Name, messwertAnalog6, messwertAnalog6Prozent, analog6Minimum, analog6Maximum);
  #endif
  #if MODUL_ANALOG7
    formatierterCode += GeneriereAnalogsensorDebugString(7, analog7Name, messwertAnalog7, messwertAnalog7Prozent, analog7Minimum, analog7Maximum);
  #endif
  #if MODUL_ANALOG8
    formatierterCode += GeneriereAnalogsensorDebugString(8, analog8Name, messwertAnalog8, messwertAnalog8Prozent, analog8Minimum, analog8Maximum);
  #endif
  formatierterCode += "<h2>Deaktivierte Module</h2><ul>";
  #if !MODUL_DHT
    formatierterCode += "<li>DHT Modul</li>";
  #endif
  #if !MODUL_DISPLAY
    formatierterCode += "<li>Display Modul</li>";
  #endif
  #if !MODUL_BODENFEUCHTE
    formatierterCode += "<li>Bodenfeuchte Modul</li>";
  #endif
  #if !MODUL_LEDAMPEL
    formatierterCode += "<li>LED Ampel Modul</li>";
  #endif
  #if !MODUL_HELLIGKEIT
    formatierterCode += "<li>Helligkeit Modul</li>";
  #endif
  #if !MODUL_WIFI
    formatierterCode += "<li>Wifi Modul</li>";
  #endif
  #if !MODUL_IFTTT
    formatierterCode += "<li>IFTTT Modul</li>";
  #endif
  #if !MODUL_ANALOG3
    formatierterCode += "<li>Analogsensor 3 Modul</li>";
  #endif
  #if !MODUL_ANALOG4
    formatierterCode += "<li>Analogsensor 4 Modul</li>";
  #endif
  #if !MODUL_ANALOG5
    formatierterCode += "<li>Analogsensor 5 Modul</li>";
  #endif
  #if !MODUL_ANALOG6
    formatierterCode += "<li>Analogsensor 6 Modul</li>";
  #endif
  #if !MODUL_ANALOG7
    formatierterCode += "<li>Analogsensor 7 Modul</li>";
  #endif
  #if !MODUL_ANALOG8
    formatierterCode += "<li>Analogsensor 8 Modul</li>";
  #endif
  formatierterCode += "</ul>";
  formatierterCode += "<h2>Links</h2>";
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
/*
 * Funktion: Void WebseiteAdminAusgeben()
 * Gibt die Administrationsseite des Webservers aus.
 */
void WebseiteAdminAusgeben() {
   #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebsiteAdminAusgeben()"));
  #endif
  #include "modul_wifi_bilder.h"
  #include "modul_wifi_header.h"
  #include "modul_wifi_footer.h"
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

/*
 * Funktion: Void WebseiteSetzeVariablen()
 * Übernimmt die Änderungen, welche auf der Administrationsseite gemacht wurden.
 */
void WebseiteSetzeVariablen() {
  #include "modul_wifi_bilder.h" // Bilder die auf der Seite verwendet werden
  #include "modul_wifi_header.h" // Kopf der HTML-Seite
  #include "modul_wifi_footer.h" // Fuß der HTML-Seite
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

/*
 * Funktion: WifiSetup()
 * Verbindet das WLAN
 */
String WifiSetup(String hostname){
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WifiSetup()"));
  #endif
// WLAN Verbindung herstellen
  WiFi.mode(WIFI_OFF); // WLAN ausschalten
  String ip = "keine WLAN Verbindung."; // Initialisierung der IP Adresse mit Fehlermeldung
  if ( !wifiAp ) { // falls kein eigener Accesspoint aufgemacht werden soll wird sich mit dem definierten WLAN verbunden
    WiFi.mode(WIFI_AP_STA); // WLAN als Client und Accesspoint
    if (WiFi.status() == WL_CONNECTED) { // falls WLAN bereits verbunden ist
      Serial.println("WLAN war verbunden");
    }
    WiFi.begin(wifiSsid, wifiPassword); // WLAN Verbindung herstellen
    int i=0; // Es wird nur 30 mal versucht, eine WLAN Verbindung aufzubauen
    Serial.print("WLAN-Verbindungsversuch: ");
    while (!(WiFi.status() == WL_CONNECTED) && i<30) { // solange keine WLAN Verbindung besteht und i kleiner als 30 ist
        Serial.print(i);
        Serial.println(" von 30.");
        delay(1000); // 1 Sekunde warten
        i++; // i um 1 erhöhen
    }
    if (WiFi.status() != WL_CONNECTED) { // falls nach 30 Versuchen keine WLAN Verbindung besteht
      Serial.println("Keine WLAN-Verbindung möglich.");
    }
    // Nun sollte WLAN verbunden sein
    Serial.print("meine IP: "); // IP Adresse ausgeben
    Serial.println(WiFi.localIP());
    ip = WiFi.localIP().toString(); // IP Adresse in Variable schreiben
  } else { // ansonsten wird hier das WLAN erstellt
    Serial.print("Konfiguriere soft-AP ... ");
    boolean result = false; // Variable für den Erfolg des Aufbaus des Accesspoints
    if ( wifiApPasswortAktiviert ) { // Falls ein WLAN mit Passwort erstellt werden soll
      result = WiFi.softAP(wifiApSsid, wifiApPasswort ); // WLAN mit Passwort erstellen
    } else { // ansonsten WLAN ohne Passwort
      result = WiFi.softAP(wifiApSsid); // WLAN ohne Passwort erstellen
    }
    Serial.print(F("Accesspoint wurde "));
    if( !result ) { // falls der Accesspoint nicht erfolgreich aufgebaut wurde
      Serial.println(F("NICHT "));
    }
    Serial.println(F("erfolgreich aufgebaut!"));
    Serial.print("meine IP: ");
    Serial.println(WiFi.softAPIP()); // IP Adresse ausgeben
    ip = WiFi.softAPIP().toString(); // IP Adresse in Variable schreiben
  }

  // DNS Namensauflösung aktivieren:
  if (MDNS.begin(hostname)) { // falls Namensauflösung erfolgreich eingerichtet wurde
    Serial.print("Gerät unter ");
    Serial.print(hostname);
    Serial.println(".local erreichbar.");
    MDNS.addService("http", "tcp", 80); // Webserver unter Port 80 bekannt machen
  } else { // falls Namensauflösung nicht erfolgreich eingerichtet wurde
    Serial.println("Fehler bein Einrichten der Namensauflösung.");
  }
  Webserver.on("/", HTTP_GET, WebseiteStartAusgeben);
  Webserver.on("/admin.html", HTTP_GET, WebseiteAdminAusgeben);
  Webserver.on("/debug.html", HTTP_GET, WebseiteDebugAusgeben);
  Webserver.on("/setzeVariablen", HTTP_POST, WebseiteSetzeVariablen);
  Webserver.begin(); // Webserver starten
  return ip; // IP Adresse zurückgeben
}
