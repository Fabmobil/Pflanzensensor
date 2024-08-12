void WebseiteDebugAusgeben() {
  Serial.println(wifiPassword1);
  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent(F("<h2>Debug-Informationen</h2>\n"));

  // Allgemeine Informationen
  Webserver.sendContent(F("<h3>Allgemeine Informationen</h3>\n<div class=\"weiss\">\n<ul>\n"));
  Webserver.sendContent(F("<li>Anzahl Module: "));
  Webserver.sendContent(String(module));
  Webserver.sendContent(F("</li>\n<li>Anzahl Neustarts: "));
  Webserver.sendContent(String(neustarts));
  Webserver.sendContent(F("</li>\n<li>Freier HEAP: "));
  Webserver.sendContent(String(ESP.getFreeHeap()));
  Webserver.sendContent(F(" Bytes</li>\n"));
  Webserver.sendContent(F("</ul>\n</div>\n"));

  #if MODUL_DHT
    Webserver.sendContent(F("<h3>DHT Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Lufttemperatur Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(lufttemperaturWebhook));
    Webserver.sendContent(F("</li>\n<li>Lufttemperatur: "));
    Webserver.sendContent(String(lufttemperaturMesswert));
    Webserver.sendContent(F("</li>\n<li>Luftfeuchte Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(luftfeuchteWebhook));
    Webserver.sendContent(F("</li>\n<li>Luftfeuchte: "));
    Webserver.sendContent(String(luftfeuchteMesswert));
    Webserver.sendContent(F("</li>\n<li>DHT Pin: "));
    Webserver.sendContent(String(dhtPin));
    Webserver.sendContent(F("</li>\n<li>DHT Sensortyp: "));
    Webserver.sendContent(String(dhtSensortyp));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_DISPLAY
    Webserver.sendContent(F("<h3>Display Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Display angeschalten?: "));
    Webserver.sendContent(String(displayAn));
    Webserver.sendContent(F("</li>\n<li>Aktives Displaybild: "));
    Webserver.sendContent(String(status));
    Webserver.sendContent(F("</li>\n<li>Breite in Pixel: "));
    Webserver.sendContent(String(displayBreite));
    Webserver.sendContent(F("</li>\n<li>Hoehe in Pixel: "));
    Webserver.sendContent(String(displayHoehe));
    Webserver.sendContent(F("</li>\n<li>Adresse: "));
    Webserver.sendContent(String(displayAdresse));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_BODENFEUCHTE
    Webserver.sendContent(F("<h3>Bodenfeuchte Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Bodenfeuchtesensor Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(bodenfeuchteWebhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(bodenfeuchteMesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert absolut: "));
    Webserver.sendContent(String(bodenfeuchteMesswert));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_LEDAMPEL
    Webserver.sendContent(F("<h3>LEDAmpel Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>LED Ampel angeschalten?: "));
    Webserver.sendContent(String(ampelAn));
    Webserver.sendContent(F("</li>\n<li>Modus: "));
    Webserver.sendContent(String(ampelModus));
    Webserver.sendContent(F("</li>\n<li>ampelUmschalten: "));
    Webserver.sendContent(String(ampelUmschalten));
    Webserver.sendContent(F("</li>\n<li>Pin gruene LED: "));
    Webserver.sendContent(String(ampelPinGruen));
    Webserver.sendContent(F("</li>\n<li>Pin gelbe LED: "));
    Webserver.sendContent(String(ampelPinGelb));
    Webserver.sendContent(F("</li>\n<li>Pin rote LED: "));
    Webserver.sendContent(String(ampelPinRot));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_HELLIGKEIT
    Webserver.sendContent(F("<h3>Helligkeit Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Helligkeitssensor Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(helligkeitWebhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(helligkeitMesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert absolut: "));
    Webserver.sendContent(String(helligkeitMesswert));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_WIFI
    Webserver.sendContent(F("<h3>Wifi Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Hostname: "));
    Webserver.sendContent(wifiHostname);
    Webserver.sendContent(F(".local</li>\n"));
    if (!wifiAp) {
      Webserver.sendContent(F("<li>SSID 1: "));
      Webserver.sendContent(wifiSsid1);
      Webserver.sendContent(F("</li>\n<li>Passwort 1: "));
      Webserver.sendContent(wifiPassword1);
      Webserver.sendContent(F("</li>\n<li>SSID 2: "));
      Webserver.sendContent(wifiSsid2);
      Webserver.sendContent(F("</li>\n<li>Passwort 2: "));
      Webserver.sendContent(wifiPassword2);
      Webserver.sendContent(F("</li>\n<li>SSID 3: "));
      Webserver.sendContent(wifiSsid3);
      Webserver.sendContent(F("</li>\n<li>Passwort 3: "));
      Webserver.sendContent(wifiPassword3);
      Webserver.sendContent(F("</li>\n"));
    } else {
      Webserver.sendContent(F("<li>Name des WLANs: "));
      Webserver.sendContent(String(wifiApSsid));
      Webserver.sendContent(F("</li>\n<li>Passwort: "));
      Webserver.sendContent(wifiApPasswortAktiviert ? String(wifiApPasswort) : F("WLAN ohne Passwortschutz!"));
      Webserver.sendContent(F("</li>\n"));
    }
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_WEBHOOK
    Webserver.sendContent(F("<h3>Webhook Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Webhook Alarm angeschalten?: "));
    Webserver.sendContent(String(webhookAn));
    Webserver.sendContent(F("</li>\n<li>Webhook Alarmierungsfrequenz: "));
    Webserver.sendContent(String(webhookFrequenz));
    Webserver.sendContent(F(" Stunden</li>\n<li>Webhook Pingfrequenz: "));
    Webserver.sendContent(String(webhookPingFrequenz));
    Webserver.sendContent(F(" Stunden</li>\n<li>Webhook Domain: "));
    Webserver.sendContent(webhookDomain);
    Webserver.sendContent(F("</li>\n<li>Webhook URL: "));
    Webserver.sendContent(webhookPfad);
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_ANALOG3
    Webserver.sendContent(F("<h3>Analogsensor 3 Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Sensorname: "));
    Webserver.sendContent(analog3Name);
    Webserver.sendContent(F("</li>\n<li>Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(analog3Webhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(analog3MesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert: "));
    Webserver.sendContent(String(analog3Messwert));
    Webserver.sendContent(F("</li>\n<li>Minimalwert: "));
    Webserver.sendContent(String(analog3Minimum));
    Webserver.sendContent(F("</li>\n<li>Maximalwert: "));
    Webserver.sendContent(String(analog3Maximum));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_ANALOG4
    Webserver.sendContent(F("<h3>Analogsensor 4 Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Sensorname: "));
    Webserver.sendContent(analog4Name);
    Webserver.sendContent(F("</li>\n<li>Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(analog4Webhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(analog4MesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert: "));
    Webserver.sendContent(String(analog4Messwert));
    Webserver.sendContent(F("</li>\n<li>Minimalwert: "));
    Webserver.sendContent(String(analog4Minimum));
    Webserver.sendContent(F("</li>\n<li>Maximalwert: "));
    Webserver.sendContent(String(analog4Maximum));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_ANALOG5
    Webserver.sendContent(F("<h3>Analogsensor 5 Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Sensorname: "));
    Webserver.sendContent(analog5Name);
    Webserver.sendContent(F("</li>\n<li>Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(analog5Webhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(analog5MesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert: "));
    Webserver.sendContent(String(analog5Messwert));
    Webserver.sendContent(F("</li>\n<li>Minimalwert: "));
    Webserver.sendContent(String(analog5Minimum));
    Webserver.sendContent(F("</li>\n<li>Maximalwert: "));
    Webserver.sendContent(String(analog5Maximum));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_ANALOG6
    Webserver.sendContent(F("<h3>Analogsensor 6 Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Sensorname: "));
    Webserver.sendContent(analog6Name);
    Webserver.sendContent(F("</li>\n<li>Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(analog6Webhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(analog6MesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert: "));
    Webserver.sendContent(String(analog6Messwert));
    Webserver.sendContent(F("</li>\n<li>Minimalwert: "));
    Webserver.sendContent(String(analog6Minimum));
    Webserver.sendContent(F("</li>\n<li>Maximalwert: "));
    Webserver.sendContent(String(analog6Maximum));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_ANALOG7
    Webserver.sendContent(F("<h3>Analogsensor 7 Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Sensorname: "));
    Webserver.sendContent(analog7Name);
    Webserver.sendContent(F("</li>\n<li>Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(analog7Webhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(analog7MesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert: "));
    Webserver.sendContent(String(analog7Messwert));
    Webserver.sendContent(F("</li>\n<li>Minimalwert: "));
    Webserver.sendContent(String(analog7Minimum));
    Webserver.sendContent(F("</li>\n<li>Maximalwert: "));
    Webserver.sendContent(String(analog7Maximum));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  #if MODUL_ANALOG8
    Webserver.sendContent(F("<h3>Analogsensor 8 Modul</h3>\n<div class=\"weiss\">\n<ul>\n"));
    Webserver.sendContent(F("<li>Sensorname: "));
    Webserver.sendContent(analog8Name);
    Webserver.sendContent(F("</li>\n<li>Webhook Alarm aktiviert?: "));
    Webserver.sendContent(String(analog8Webhook));
    Webserver.sendContent(F("</li>\n<li>Messwert Prozent: "));
    Webserver.sendContent(String(analog8MesswertProzent));
    Webserver.sendContent(F("</li>\n<li>Messwert: "));
    Webserver.sendContent(String(analog8Messwert));
    Webserver.sendContent(F("</li>\n<li>Minimalwert: "));
    Webserver.sendContent(String(analog8Minimum));
    Webserver.sendContent(F("</li>\n<li>Maximalwert: "));
    Webserver.sendContent(String(analog8Maximum));
    Webserver.sendContent(F("</li>\n"));
    Webserver.sendContent(F("</ul>\n</div>\n"));
  #endif

  Webserver.sendContent(F("<h2>Deaktivierte Module</h2>\n<div class=\"weiss\">\n<ul>\n"));
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
    Webserver.sendContent(F("<li>IFTTT Modul</li>\n"));
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

  Webserver.sendContent(F(
    "<h2>Links</h2>\n"
    "<div class=\"weiss\">\n"
    "<ul>\n"
    "<li><a href=\"/\">zur Startseite</a></li>\n"
    "<li><a href=\"/admin.html\">zur Administrationsseite</a></li>\n"));

  #if MODUL_DEBUG
    Webserver.sendContent(F("<li><a href=\"/debug.html\">zur Anzeige der Debuginformationen</a></li>\n"));
  #endif

  Webserver.sendContent(F(
    "<li><a href=\"https://www.github.com/Fabmobil/Pflanzensensor\" target=\"_blank\">"
    "<img src=\"/Bilder/logoGithub.png\">&nbspRepository mit dem Quellcode und der Dokumentation</a></li>\n"
    "<li><a href=\"https://www.fabmobil.org\" target=\"_blank\">"
    "<img src=\"/Bilder/logoFabmobil.png\">&nbspHomepage</a></li>\n"
    "</ul>\n"
    "</div>\n"));

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();
}

