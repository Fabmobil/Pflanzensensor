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
  Webserver.sendContent(F("<h3>"));
  Webserver.sendContent(titel);
  Webserver.sendContent(F("</h3>\n<div class=\"tuerkis\">\n"));
  Webserver.sendContent(inhalt);
  Webserver.sendContent(F("</div>\n"));
}

String generiereAnalogSensorInfo(const String& name, bool webhook, int messwertProzent, int messwert, int minimum, int maximum) {
  return F("<ul><li>Sensorname: ") + name +
         F("</li><li>Webhook Alarm aktiviert?: ") + String(webhook) +
         F("</li><li>Messwert Prozent: ") + String(messwertProzent) +
         F("</li><li>Messwert: ") + String(messwert) +
         F("</li><li>Minimalwert: ") + String(minimum) +
         F("</li><li>Maximalwert: ") + String(maximum) + F("</li></ul>");
}

void WebseiteDebugAusgeben() {
  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  sendeHtmlHeader(Webserver, false);

  // Log-Abschnitt
  Webserver.sendContent(F("<h2>Logs</h2><div class=\"tuerkis\">"));
  Webserver.sendContent(F("<form action='/SetzeLogLevel' method='post'><select name='logLevel'>"));
  Webserver.sendContent(F("<option value='DEBUG'"));
  if (logger.getLogLevel() == LogLevel::DEBUG) Webserver.sendContent(F(" selected"));
  Webserver.sendContent(F(">DEBUG</option><option value='INFO'"));
  if (logger.getLogLevel() == LogLevel::INFO) Webserver.sendContent(F(" selected"));
  Webserver.sendContent(F(">INFO</option><option value='WARNING'"));
  if (logger.getLogLevel() == LogLevel::WARNING) Webserver.sendContent(F(" selected"));
  Webserver.sendContent(F(">WARNING</option><option value='ERROR'"));
  if (logger.getLogLevel() == LogLevel::ERROR) Webserver.sendContent(F(" selected"));
  Webserver.sendContent(F(">ERROR</option></select><input type='submit' value='Setze Log Level'></form>"));
  Webserver.sendContent(F("<div id='logTable'>"));
  Webserver.sendContent(logger.getLogsAsHtmlTable(logAnzahlWebseite));
  Webserver.sendContent(F("</div></div>"));

  // JavaScript einfügen
  Webserver.sendContent_P(DEBUG_JAVASCRIPT);

  Webserver.sendContent(F("<h2>Debug-Informationen</h2>\n"));

  // Allgemeine Informationen
  String allgemeineInfo = F("<ul><li>Anzahl Module: ") + String(module) +
                          F("</li><li>Anzahl Neustarts: ") + String(neustarts) +
                          F("</li><li>Freier HEAP: ") + String(ESP.getFreeHeap()) + F(" Bytes</li></ul>");
  sendeDebugAbschnitt(F("Allgemeine Informationen"), allgemeineInfo);

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
    String dhtInfo = F("<ul><li>Lufttemperatur Webhook Alarm aktiviert?: ") + String(lufttemperaturWebhook) +
                     F("</li><li>Lufttemperatur: ") + String(lufttemperaturMesswert) +
                     F("</li><li>Luftfeuchte Webhook Alarm aktiviert?: ") + String(luftfeuchteWebhook) +
                     F("</li><li>Luftfeuchte: ") + String(luftfeuchteMesswert) +
                     F("</li><li>DHT Pin: ") + String(dhtPin) +
                     F("</li><li>DHT Sensortyp: ") + String(dhtSensortyp) + F("</li></ul>");
    sendeDebugAbschnitt(F("DHT Modul"), dhtInfo);
  #endif

  #if MODUL_DISPLAY
    String displayInfo = F("<ul><li>Display angeschalten?: ") + String(displayAn) +
                         F("</li><li>Breite in Pixel: ") + String(displayBreite) +
                         F("</li><li>Hoehe in Pixel: ") + String(displayHoehe) +
                         F("</li><li>Adresse: ") + String(displayAdresse) + F("</li></ul>");
    sendeDebugAbschnitt(F("Display Modul"), displayInfo);
  #endif

  #if MODUL_LEDAMPEL
    String ledAmpelInfo = F("<ul><li>LED Ampel angeschalten?: ") + String(ampelAn) +
                          F("</li><li>Modus: ") + String(ampelModus) +
                          F("</li><li>Pin gruene LED: ") + String(ampelPinGruen) +
                          F("</li><li>Pin gelbe LED: ") + String(ampelPinGelb) +
                          F("</li><li>Pin rote LED: ") + String(ampelPinRot) + F("</li></ul>");
    sendeDebugAbschnitt(F("LEDAmpel Modul"), ledAmpelInfo);
  #endif

  #if MODUL_WIFI
    String wifiInfo = F("<ul><li>Hostname: ") + wifiHostname + F(".local</li>");
    if (!wifiAp) {
      wifiInfo += F("<li>SSID 1: ") + wifiSsid1 +
                  F("</li><li>Passwort 1: ") + wifiPasswort1 +
                  F("</li><li>SSID 2: ") + wifiSsid2 +
                  F("</li><li>Passwort 2: ") + wifiPasswort2 +
                  F("</li><li>SSID 3: ") + wifiSsid3 +
                  F("</li><li>Passwort 3: ") + wifiPasswort3 + F("</li>");
    } else {
      wifiInfo += F("<li>Name des WLANs: ") + String(wifiApSsid) +
                  F("</li><li>Passwort: ") + (wifiApPasswortAktiviert ? String(wifiApPasswort) : F("WLAN ohne Passwortschutz!")) + F("</li>");
    }
    wifiInfo += F("</ul>");
    sendeDebugAbschnitt(F("Wifi Modul"), wifiInfo);
  #endif

  #if MODUL_WEBHOOK
    String webhookInfo = F("<ul><li>Webhook Alarm angeschalten?: ") + String(webhookAn) +
                         F("</li><li>Webhook Alarmierungsfrequenz: ") + String(webhookFrequenz) +
                         F(" Stunden</li><li>Webhook Pingfrequenz: ") + String(webhookPingFrequenz) +
                         F(" Stunden</li><li>Webhook Domain: ") + webhookDomain +
                         F("</li><li>Webhook URL: ") + webhookPfad + F("</li></ul>");
    sendeDebugAbschnitt(F("Webhook Modul"), webhookInfo);
  #endif

   // Deaktivierte Module
  Webserver.sendContent(F("<h2>Deaktivierte Module</h2>\n<div class=\"tuerkis\">\n<ul>\n"));
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
  Webserver.sendContent(F(
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
    "</div>\n"));

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();
}
