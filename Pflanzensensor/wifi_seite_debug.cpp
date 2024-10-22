/**
 * @file wifi_seite_debug.cpp
 * @brief Implementierung der Debug-Informationsseite des Pflanzensensors
 * @author Tommy, Claude
 * @date 2023-09-20
 *
 * Diese Datei enthält die Implementierung der Funktionen zur Generierung der Debug-Informationsseite
 * des Pflanzensensors.
 */

#include "wifi_seite_debug.h"
#include "einstellungen.h"
#include "passwoerter.h"
#include "wifi_header.h"
#include "wifi_footer.h"
#include "logger.h"

// JavaScript-Code im Flash-Speicher ablegen
const char PROGMEM DEBUG_JAVASCRIPT[] = R"=====(
<script>
function updateLogTable() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("logTable").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/neuesteLogs", true);
  xhttp.send();
}

document.querySelector('form').addEventListener('submit', function(e) {
  e.preventDefault();
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("logTable").innerHTML = this.responseText;
    }
  };
  xhttp.open("POST", "/SetzeLogLevel", true);
  xhttp.send(new FormData(this));
});
setInterval(updateLogTable, 5000);
updateLogTable();
</script>
)=====";

void sendeDebugAbschnitt(const __FlashStringHelper* titel, const String& inhalt) {
  char buffer[500];
  snprintf_P(buffer, sizeof(buffer), PSTR("<h3>%s</h3>\n<div class=\"tuerkis\">\n%s</div>\n"),
             reinterpret_cast<const char*>(titel), inhalt.c_str());
  Webserver.sendContent(buffer);
}

String generiereAnalogSensorInfo(const String& name, bool webhook, int messwertProzent, int messwert, int minimum, int maximum) {
  char buffer[500];
  snprintf_P(buffer, sizeof(buffer), PSTR("<ul><li>Sensorname: %s</li><li>Webhook Alarm aktiviert?: %s</li>"
                                          "<li>Messwert Prozent: %d</li><li>Messwert: %d</li>"
                                          "<li>Minimalwert: %d</li><li>Maximalwert: %d</li></ul>"),
             name.c_str(), webhook ? "Ja" : "Nein", messwertProzent, messwert, minimum, maximum);
  return String(buffer);
}

void WebseiteDebugAusgeben() {
  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  sendeHtmlHeader(Webserver, false, false);

  // Log-Abschnitt
  static const char PROGMEM logSection[] =
    "<h2>Logs</h2><div class=\"tuerkis\">"
    "<form action='/SetzeLogLevel' method='post'><select name='logLevel'>"
    "<option value='DEBUG'%s>DEBUG</option>"
    "<option value='INFO'%s>INFO</option>"
    "<option value='WARNING'%s>WARNING</option>"
    "<option value='ERROR'%s>ERROR</option>"
    "</select><input type='submit' value='Setze Log Level'></form>"
    "<div id='logTable'>";

  char buffer[1000];
  snprintf_P(buffer, sizeof(buffer), logSection,
             logger.LeseLogLevel() == LogLevel::DEBUG ? " selected" : "",
             logger.LeseLogLevel() == LogLevel::INFO ? " selected" : "",
             logger.LeseLogLevel() == LogLevel::WARNING ? " selected" : "",
             logger.LeseLogLevel() == LogLevel::ERROR ? " selected" : "");
  Webserver.sendContent(buffer);

  Webserver.sendContent(logger.LogsAlsHtmlTabelle(MAX_LOG_EINTRAEGE));
  Webserver.sendContent(F("</div></div>"));

  // JavaScript einfügen
  Webserver.sendContent_P(DEBUG_JAVASCRIPT);

  Webserver.sendContent(F("<h2>Debug-Informationen</h2>\n"));

  // Allgemeine Informationen
  snprintf_P(buffer, sizeof(buffer), PSTR("<ul><li>Anzahl Module: %d</li>"
                                          "<li>Anzahl Neustarts: %d</li>"
                                          "<li>Freier HEAP: %d Bytes</li></ul>"),
             module, neustarts, ESP.getFreeHeap());
  sendeDebugAbschnitt(F("Allgemeine Informationen"), buffer);

  // Analogsensoren
  #if MODUL_HELLIGKEIT
    sendeDebugAbschnitt(F("Helligkeit Modul"),
      generiereAnalogSensorInfo(helligkeitName, helligkeitWebhook, helligkeitMesswertProzent, helligkeitMesswert, helligkeitMinimum, helligkeitMaximum));
  #endif

  #if MODUL_BODENFEUCHTE
    sendeDebugAbschnitt(F("Bodenfeuchte Modul"),
      generiereAnalogSensorInfo(bodenfeuchteName, bodenfeuchteWebhook, bodenfeuchteMesswertProzent, bodenfeuchteMesswert, bodenfeuchteMinimum, bodenfeuchteMaximum));
  #endif

  #if MODUL_ANALOG3
    sendeDebugAbschnitt(F("Analogsensor 3 Modul"),
      generiereAnalogSensorInfo(analog3Name, analog3Webhook, analog3MesswertProzent, analog3Messwert, analog3Minimum, analog3Maximum));
  #endif

  #if MODUL_ANALOG4
    sendeDebugAbschnitt(F("Analogsensor 4 Modul"),
      generiereAnalogSensorInfo(analog4Name, analog4Webhook, analog4MesswertProzent, analog4Messwert, analog4Minimum, analog4Maximum));
  #endif

  #if MODUL_ANALOG5
    sendeDebugAbschnitt(F("Analogsensor 5 Modul"),
      generiereAnalogSensorInfo(analog5Name, analog5Webhook, analog5MesswertProzent, analog5Messwert, analog5Minimum, analog5Maximum));
  #endif

  #if MODUL_ANALOG6
    sendeDebugAbschnitt(F("Analogsensor 6 Modul"),
      generiereAnalogSensorInfo(analog6Name, analog6Webhook, analog6MesswertProzent, analog6Messwert, analog6Minimum, analog6Maximum));
  #endif

  #if MODUL_ANALOG7
    sendeDebugAbschnitt(F("Analogsensor 7 Modul"),
      generiereAnalogSensorInfo(analog7Name, analog7Webhook, analog7MesswertProzent, analog7Messwert, analog7Minimum, analog7Maximum));
  #endif

  #if MODUL_ANALOG8
    sendeDebugAbschnitt(F("Analogsensor 8 Modul"),
      generiereAnalogSensorInfo(analog8Name, analog8Webhook, analog8MesswertProzent, analog8Messwert, analog8Minimum, analog8Maximum));
  #endif

  // Andere Module
  #if MODUL_DHT
    snprintf_P(buffer, sizeof(buffer), PSTR("<ul><li>Lufttemperatur Webhook Alarm aktiviert?: %s</li>"
                                            "<li>Lufttemperatur: %.2f</li>"
                                            "<li>Luftfeuchte Webhook Alarm aktiviert?: %s</li>"
                                            "<li>Luftfeuchte: %.2f</li>"
                                            "<li>DHT Pin: %d</li>"
                                            "<li>DHT Sensortyp: %d</li></ul>"),
               lufttemperaturWebhook ? "Ja" : "Nein", lufttemperaturMesswert,
               luftfeuchteWebhook ? "Ja" : "Nein", luftfeuchteMesswert,
               dhtPin, dhtSensortyp);
    sendeDebugAbschnitt(F("DHT Modul"), buffer);
  #endif

  #if MODUL_DISPLAY
    snprintf_P(buffer, sizeof(buffer), PSTR("<ul><li>Display angeschalten?: %s</li>"
                                            "<li>Breite in Pixel: %d</li>"
                                            "<li>Hoehe in Pixel: %d</li>"
                                            "<li>Adresse: %d</li></ul>"),
               displayAn ? "Ja" : "Nein", displayBreite, displayHoehe, displayAdresse);
    sendeDebugAbschnitt(F("Display Modul"), buffer);
  #endif

  #if MODUL_LEDAMPEL
    snprintf_P(buffer, sizeof(buffer), PSTR("<ul><li>LED Ampel angeschalten?: %s</li>"
                                            "<li>Modus: %d</li>"
                                            "<li>Pin gruene LED: %d</li>"
                                            "<li>Pin gelbe LED: %d</li>"
                                            "<li>Pin rote LED: %d</li></ul>"),
               ampelAn ? "Ja" : "Nein", ampelModus, ampelPinGruen, ampelPinGelb, ampelPinRot);
    sendeDebugAbschnitt(F("LEDAmpel Modul"), buffer);
  #endif

  #if MODUL_WIFI
    strcpy_P(buffer, PSTR("<ul><li>Hostname: "));
    strcat(buffer, wifiHostname.c_str());
    strcat_P(buffer, PSTR(".local</li>"));
    if (!wifiAp) {
      char tempBuffer[250];
      snprintf_P(tempBuffer, sizeof(tempBuffer), PSTR("<li>SSID 1: %s</li>"
                                                      "<li>SSID 2: %s</li>"
                                                      "<li>SSID 3: %s</li>"),
                 wifiSsid1.c_str(),
                 wifiSsid2.c_str(),
                 wifiSsid3.c_str());
      strcat(buffer, tempBuffer);
    } else {
      char tempBuffer[250];
      snprintf_P(tempBuffer, sizeof(tempBuffer), PSTR("<li>Name des WLANs: %s</li>"
                                                      "<li>Passwort: %s</li>"),
                 wifiApSsid.c_str(), wifiApPasswortAktiviert ? wifiApPasswort.c_str() : "WLAN ohne Passwortschutz!");
      strcat(buffer, tempBuffer);
    }
    strcat_P(buffer, PSTR("</ul>"));
    sendeDebugAbschnitt(F("Wifi Modul"), buffer);
  #endif

  #if MODUL_WEBHOOK
    snprintf_P(buffer, sizeof(buffer), PSTR("<ul><li>Webhook Alarm angeschalten?: %s</li>"
                                            "<li>Webhook Alarmierungsfrequenz: %d Stunden</li>"
                                            "<li>Webhook Pingfrequenz: %d Stunden</li>"
                                            "<li>Webhook Domain: %s</li>"
                                            "<li>Webhook URL: %s</li></ul>"),
               webhookAn ? "Ja" : "Nein", webhookFrequenz, webhookPingFrequenz,
               webhookDomain.c_str(), webhookPfad.c_str());
    sendeDebugAbschnitt(F("Webhook Modul"), buffer);
  #endif

   // Deaktivierte Module
  static const char PROGMEM deactivatedModules[] = "<h2>Deaktivierte Module</h2>\n<div class=\"tuerkis\">\n<ul>\n";
  Webserver.sendContent_P(deactivatedModules);

  #if !MODUL_DHT
    Webserver.sendContent(F("<li>DHT Modul</li>\n"));
  #endif
  #if !MODUL_DISPLAY
    Webserver.sendContent(F("<li>Display Modul</li>\n"));
  #endif
  #if !MODUL_BODENFEUCHTE
    Webserver.sendContent(F("<li>Bodenfeuchte Modul</li>\n"));
  #endif
  #if !MODUL_LEDAMPEL
    Webserver.sendContent(F("<li>LED Ampel Modul</li>\n"));
  #endif
  #if !MODUL_HELLIGKEIT
    Webserver.sendContent(F("<li>Helligkeit Modul</li>\n"));
  #endif
  #if !MODUL_WIFI
    Webserver.sendContent(F("<li>Wifi Modul</li>\n"));
  #endif
  #if !MODUL_WEBHOOK
    Webserver.sendContent(F("<li>Webhook Modul</li>\n"));
  #endif
  #if !MODUL_ANALOG3
    Webserver.sendContent(F("<li>Analogsensor 3 Modul</li>\n"));
  #endif
  #if !MODUL_ANALOG4
    Webserver.sendContent(F("<li>Analogsensor 4 Modul</li>\n"));
  #endif
  #if !MODUL_ANALOG5
    Webserver.sendContent(F("<li>Analogsensor 5 Modul</li>\n"));
  #endif
  #if !MODUL_ANALOG6
    Webserver.sendContent(F("<li>Analogsensor 6 Modul</li>\n"));
  #endif
  #if !MODUL_ANALOG7
    Webserver.sendContent(F("<li>Analogsensor 7 Modul</li>\n"));
  #endif
  #if !MODUL_ANALOG8
    Webserver.sendContent(F("<li>Analogsensor 8 Modul</li>\n"));
  #endif
  Webserver.sendContent(F("</ul>\n</div>\n"));

   // Links
  static const char PROGMEM links[] =
    "<h2>Links</h2>\n"
    "<div class=\"tuerkis\">\n"
    "<ul>\n"
    "<li><a href=\"/\">zur Startseite</a></li>\n"
    "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"
    "<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"
    "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
    "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
    "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
    "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
    "</ul>\n"
    "</div>\n";
  Webserver.sendContent_P(links);

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();
}
