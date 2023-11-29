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


/* Funktion: WebseiteDebugAusgeben()
 * Gibt die Debugseite des Webservers aus.
 */
void WebseiteDebugAusgeben() {
  #include "wifi_bilder.h"
  #include "wifi_header.h"
  #include "wifi_footer.h"
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
    formatierterCode += lufttemperaturMesswert;
    formatierterCode += "</li>";
    formatierterCode += "<li>Luftfeuchte: ";
    formatierterCode += luftfeuchteMesswert;
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
    formatierterCode += bodenfeuchteMesswertProzent;
    formatierterCode += "</li>";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += bodenfeuchteMesswert;
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
    formatierterCode += "</ul>";
  #endif

  #if MODUL_HELLIGKEIT
    formatierterCode += "<h3>Helligkeit Modul</h3>";
    formatierterCode += "<ul>";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += helligkeitMesswertProzent;
    formatierterCode += "</li>";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += helligkeitMesswert;
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
