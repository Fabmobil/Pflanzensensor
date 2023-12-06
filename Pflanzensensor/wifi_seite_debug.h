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
  analogsensorDebugString += "<h3>Analogsensor " + String(sensorNummer) + " Modul</h3>\n";
  analogsensorDebugString += "<div class=\"weiss\">\n";
  analogsensorDebugString += "<ul>\n";
  analogsensorDebugString += "<li>Sensorname: " + String(sensorName) + "</li>\n";
  analogsensorDebugString += "<li>Messwert Prozent: " + String(messwertProzent) + "</li>\n";
  analogsensorDebugString += "<li>Messwert: " + String(messwert) + "</li>\n";
  analogsensorDebugString += "<li>Minimalwert: " + String(minimum) + "</li>\n";
  analogsensorDebugString += "<li>Maximalwert: " + String(maximum) + "</li>\n</ul>\n";
  analogsensorDebugString += "</div>\n";
  return analogsensorDebugString;
}


/* Funktion: WebseiteDebugAusgeben()
 * Gibt die Debugseite des Webservers aus.
 */
void WebseiteDebugAusgeben() {
  #include "wifi_header.h"
  #include "wifi_footer.h"
  String formatierterCode = htmlHeader;
  formatierterCode += "<h2>Debug-Informationen</h2>\n";
  formatierterCode += "<div class=\"weiss\">\n";
  formatierterCode += "<ul>\n";
  formatierterCode += "<li>Anzahl Module: ";
  formatierterCode += module;
  formatierterCode += "</li>\n";
  formatierterCode += "</ul>\n";
  formatierterCode += "</div>\n";

  #if MODUL_DHT
    formatierterCode += "<h3>DHT Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>Lufttemperatur: ";
    formatierterCode += lufttemperaturMesswert;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Luftfeuchte: ";
    formatierterCode += luftfeuchteMesswert;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>DHT Pin: ";
    formatierterCode += dhtPin;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>DHT Sensortyp: ";
    formatierterCode += dhtSensortyp;
    formatierterCode += "</li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
  #endif

  #if MODUL_DISPLAY
    formatierterCode += "<h3>Display Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>Aktives Displaybild: ";
    formatierterCode += status;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Breite in Pixel: ";
    formatierterCode += displayBreite;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Hoehe in Pixel: ";
    formatierterCode += displayHoehe;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Adresse: ";
    formatierterCode += displayAdresse;
    formatierterCode += "</li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
  #endif

  #if MODUL_BODENFEUCHTE
    formatierterCode += "<h3>Bodenfeuchte Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += bodenfeuchteMesswertProzent;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += bodenfeuchteMesswert;
    formatierterCode += "</li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
 #endif

  #if MODUL_LEDAMPEL
    formatierterCode += "<h3>LEDAmpel Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>Modus: ";
    formatierterCode += ampelModus;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>ampelUmschalten: ";
    formatierterCode += ampelUmschalten;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Pin gruene LED: ";
    formatierterCode += ampelPinGruen;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Pin gelbe LED: ";
    formatierterCode += ampelPinGelb;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Pin rote LED: ";
    formatierterCode += ampelPinRot;
    formatierterCode += "</li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
  #endif

  #if MODUL_HELLIGKEIT
    formatierterCode += "<h3>Helligkeit Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += helligkeitMesswertProzent;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += helligkeitMesswert;
    formatierterCode += "</li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
  #endif

  #if MODUL_WIFI
    formatierterCode += "<h3>Wifi Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>Hostname: ";
    formatierterCode += wifiHostname;
    formatierterCode += ".local</li>\n";
    if ( wifiAp == false ) { // falls der ESP in einem anderen WLAN ist:
      formatierterCode += "<li>SSID 1: ";
      formatierterCode += wifiSsid1;
      formatierterCode += "</li>\n";
      formatierterCode += "<li>Passwort 1: ";
      formatierterCode += wifiPassword1;
      formatierterCode += "</li>\n";
      formatierterCode += "<li>SSID 2: ";
      formatierterCode += wifiSsid2;
      formatierterCode += "</li>\n";
      formatierterCode += "<li>Passwort 2: ";
      formatierterCode += wifiPassword2;
      formatierterCode += "</li>\n";
      formatierterCode += "<li>SSID 3: ";
      formatierterCode += wifiSsid3;
      formatierterCode += "</li>\n";
      formatierterCode += "<li>Passwort 3: ";
      formatierterCode += wifiPassword3;
      formatierterCode += "</li>\n";
    } else { // falls der ESP sein eigenes WLAN aufmacht:
      formatierterCode += "<li>Name des WLANs: ";
      formatierterCode += wifiApSsid;
      formatierterCode += "</li>\n";
      formatierterCode += "<li>Passwort: ";
      if ( wifiApPasswortAktiviert ) {
        formatierterCode += wifiApPasswort;
      } else {
        formatierterCode += "WLAN ohne Passwortschutz!";
      }
      formatierterCode += "</li>\n";
    }
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
  #endif
  #if MODUL_IFTTT
    formatierterCode += "<h3>IFTTT Modul</h3>\n";
    formatierterCode += "<div class=\"weiss\">\n";
    formatierterCode += "<ul>\n";
    formatierterCode += "<li>IFTTT Passwort: ";
    formatierterCode += wifiIftttPasswort;
    formatierterCode += "</li>\n";
    formatierterCode += "<li>IFTTT Ereignis: ";
    formatierterCode += wifiIftttEreignis;
    formatierterCode += "</li>\n";
    formatierterCode += "</ul>\n";
    formatierterCode += "</div>\n";
 #endif

  #if MODUL_ANALOG3
    formatierterCode += GeneriereAnalogsensorDebugString(3, analog3Name, analog3Messwert, analog3MesswertProzent, analog3Minimum, analog3Maximum);
  #endif
  #if MODUL_ANALOG4
    formatierterCode += GeneriereAnalogsensorDebugString(4, analog4Name, analog4Messwert, analog4MesswertProzent, analog4Minimum, analog4Maximum);
  #endif
  #if MODUL_ANALOG5
    formatierterCode += GeneriereAnalogsensorDebugString(5, analog5Name, analog5Messwert, analog5MesswertProzent, analog5Minimum, analog5Maximum);
  #endif
  #if MODUL_ANALOG6
    formatierterCode += GeneriereAnalogsensorDebugString(6, analog6Name, analog6Messwert, analog6MesswertProzent, analog6Minimum, analog6Maximum);
  #endif
  #if MODUL_ANALOG7
    formatierterCode += GeneriereAnalogsensorDebugString(7, analog7Name, analog7Messwert, analog7MesswertProzent, analog7Minimum, analog7Maximum);
  #endif
  #if MODUL_ANALOG8
    formatierterCode += GeneriereAnalogsensorDebugString(8, analog8Name, analog8Messwert, analog8MesswertProzent, analog8Minimum, analog8Maximum);
  #endif
  formatierterCode += "<h2>Deaktivierte Module</h2>\n";
  formatierterCode += "<div class=\"weiss\">\n";
  formatierterCode += "<ul>\n";
  #if !MODUL_DHT
    formatierterCode += "<li>DHT Modul</li>\n";
  #endif
  #if !MODUL_DISPLAY
    formatierterCode += "<li>Display Modul</li>\n";
  #endif
  #if !MODUL_BODENFEUCHTE
    formatierterCode += "<li>Bodenfeuchte Modul</li>\n";
  #endif
  #if !MODUL_LEDAMPEL
    formatierterCode += "<li>LED Ampel Modul</li>\n";
  #endif
  #if !MODUL_HELLIGKEIT
    formatierterCode += "<li>Helligkeit Modul</li>\n";
  #endif
  #if !MODUL_WIFI
    formatierterCode += "<li>Wifi Modul</li>\n";
  #endif
  #if !MODUL_IFTTT
    formatierterCode += "<li>IFTTT Modul</li>\n";
  #endif
  #if !MODUL_ANALOG3
    formatierterCode += "<li>Analogsensor 3 Modul</li>\n";
  #endif
  #if !MODUL_ANALOG4
    formatierterCode += "<li>Analogsensor 4 Modul</li>\n";
  #endif
  #if !MODUL_ANALOG5
    formatierterCode += "<li>Analogsensor 5 Modul</li>\n";
  #endif
  #if !MODUL_ANALOG6
    formatierterCode += "<li>Analogsensor 6 Modul</li>\n";
  #endif
  #if !MODUL_ANALOG7
    formatierterCode += "<li>Analogsensor 7 Modul</li>\n";
  #endif
  #if !MODUL_ANALOG8
    formatierterCode += "<li>Analogsensor 8 Modul</li>\n";
  #endif
  formatierterCode += "</ul>\n";
  formatierterCode += "</div>\n";
  formatierterCode += "<h2>Links</h2>\n";
  formatierterCode += "<div class=\"weiss\">\n";
  formatierterCode += "<ul>\n";
  formatierterCode += "<li><a href=\"/\">zur Startseite</a></li>\n";
  formatierterCode += "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n";
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
}
