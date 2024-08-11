void sendeDebugInfo(const __FlashStringHelper* titel, const String& inhalt) {
  Webserver.sendContent(F("<h3>"));
  Webserver.sendContent(titel);
  Webserver.sendContent(F("</h3>\n<div class=\"weiss\">\n<ul>\n"));
  Webserver.sendContent(inhalt);
  Webserver.sendContent(F("</ul>\n</div>\n"));
}

void sendeDebugInfo(const char* titel, const String& inhalt) {
  sendeDebugInfo(FPSTR(titel), inhalt);
}

void sendeAnalogsensorDebugInfo(int sensorNummer, const String& sensorName, int messwert, int messwertProzent, int minimum, int maximum, bool alarm) {
  String analogInfo;
  analogInfo += F("<li>Sensorname: ");
  analogInfo += sensorName;
  analogInfo += F("<li> Webhook Alarm aktiviert?: ");
  analogInfo += String(alarm);
  analogInfo += F("</li>\n<li>Messwert Prozent: ");
  analogInfo += String(messwertProzent);
  analogInfo += F("</li>\n<li>Messwert: ");
  analogInfo += String(messwert);
  analogInfo += F("</li>\n<li>Minimalwert: ");
  analogInfo += String(minimum);
  analogInfo += F("</li>\n<li>Maximalwert: ");
  analogInfo += String(maximum);
  analogInfo += F("</li>\n");

  char titelBuffer[30];
  snprintf_P(titelBuffer, sizeof(titelBuffer), PSTR("Analogsensor %d Modul"), sensorNummer);
  sendeDebugInfo(titelBuffer, analogInfo);
}

void WebseiteDebugAusgeben() {
  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(200, F("text/html"), "");

  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent(F("<h2>Debug-Informationen</h2>\n"));
  String allgemeineInfo;
  allgemeineInfo += F("<li>Anzahl Module: ");
  allgemeineInfo += String(module);
  allgemeineInfo += F("<li>Anzahl Neustarts: ");
  allgemeineInfo += String(neustarts);
  allgemeineInfo += F("<li>Freier HEAP: ");
  allgemeineInfo += String(ESP.getFreeHeap());
  allgemeineInfo += F(" Bytes</li>\n");
  sendeDebugInfo(F("Allgemeine Informationen"), allgemeineInfo);

  #if MODUL_DHT
    String dhtInfo;
    dhtInfo += F("<li>Lufttemperatur Webhook Alarm aktiviert?: ");
    dhtInfo += String(lufttemperaturWebhook);
    dhtInfo += F("<li>Lufttemperatur: ");
    dhtInfo += String(lufttemperaturMesswert);
    dhtInfo += F("<li>Luftfeuchte Webhook Alarm aktiviert?: ");
    dhtInfo += String(luftfeuchteWebhook);
    dhtInfo += F("</li>\n<li>Luftfeuchte: ");
    dhtInfo += String(luftfeuchteMesswert);
    dhtInfo += F("</li>\n<li>DHT Pin: ");
    dhtInfo += String(dhtPin);
    dhtInfo += F("</li>\n<li>DHT Sensortyp: ");
    dhtInfo += String(dhtSensortyp);
    dhtInfo += F("</li>\n");
    sendeDebugInfo(F("DHT Modul"), dhtInfo);
  #endif

  #if MODUL_DISPLAY
    String displayInfo;
    displayInfo += F("<li>Display angeschalten?: ");
    displayInfo += String(displayAn);
    displayInfo += F("<li>Aktives Displaybild: ");
    displayInfo += String(status);
    displayInfo += F("</li>\n<li>Breite in Pixel: ");
    displayInfo += String(displayBreite);
    displayInfo += F("</li>\n<li>Hoehe in Pixel: ");
    displayInfo += String(displayHoehe);
    displayInfo += F("</li>\n<li>Adresse: ");
    displayInfo += String(displayAdresse);
    displayInfo += F("</li>\n");
    sendeDebugInfo(F("Display Modul"), displayInfo);
  #endif

  #if MODUL_BODENFEUCHTE
    String bodenfeuchteInfo;
    bodenfeuchteInfo += F("<li>Bodenfeuchtesensor Webhook Alarm aktiviert?: ");
    bodenfeuchteInfo += String(bodenfeuchteWebhook);
    bodenfeuchteInfo += F("<li>Messwert Prozent: ");
    bodenfeuchteInfo += String(bodenfeuchteMesswertProzent);
    bodenfeuchteInfo += F("</li>\n<li>Messwert absolut: ");
    bodenfeuchteInfo += String(bodenfeuchteMesswert);
    bodenfeuchteInfo += F("</li>\n");
    sendeDebugInfo(F("Bodenfeuchte Modul"), bodenfeuchteInfo);
  #endif

  #if MODUL_LEDAMPEL
    String ledAmpelInfo;
    displayInfo += F("<li>LED Ampel angeschalten?: ");
    displayInfo += String(ampelAn);
    ledAmpelInfo += F("<li>Modus: ");
    ledAmpelInfo += String(ampelModus);
    ledAmpelInfo += F("</li>\n<li>ampelUmschalten: ");
    ledAmpelInfo += String(ampelUmschalten);
    ledAmpelInfo += F("</li>\n<li>Pin gruene LED: ");
    ledAmpelInfo += String(ampelPinGruen);
    ledAmpelInfo += F("</li>\n<li>Pin gelbe LED: ");
    ledAmpelInfo += String(ampelPinGelb);
    ledAmpelInfo += F("</li>\n<li>Pin rote LED: ");
    ledAmpelInfo += String(ampelPinRot);
    ledAmpelInfo += F("</li>\n");
    sendeDebugInfo(F("LEDAmpel Modul"), ledAmpelInfo);
  #endif

  #if MODUL_HELLIGKEIT
    String helligkeitInfo;
    helligkeitInfo += F("<li>Helligkeitssensor Webhook Alarm aktiviert?: ");
    helligkeitInfo += String(helligkeitWebhook);
    helligkeitInfo += F("<li>Messwert Prozent: ");
    helligkeitInfo += String(helligkeitMesswertProzent);
    helligkeitInfo += F("</li>\n<li>Messwert absolut: ");
    helligkeitInfo += String(helligkeitMesswert);
    helligkeitInfo += F("</li>\n");
    sendeDebugInfo(F("Helligkeit Modul"), helligkeitInfo);
  #endif

  #if MODUL_WIFI
    String wifiInfo;
    wifiInfo += F("<li>Hostname: ");
    wifiInfo += wifiHostname;
    wifiInfo += F(".local</li>\n");
    if (!wifiAp) {
      wifiInfo += F("<li>SSID 1: ");
      wifiInfo += wifiSsid1;
      wifiInfo += F("</li>\n<li>Passwort 1: ");
      wifiInfo += wifiPassword1;
      wifiInfo += F("</li>\n<li>SSID 2: ");
      wifiInfo += wifiSsid2;
      wifiInfo += F("</li>\n<li>Passwort 2: ");
      wifiInfo += wifiPassword2;
      wifiInfo += F("</li>\n<li>SSID 3: ");
      wifiInfo += wifiSsid3;
      wifiInfo += F("</li>\n<li>Passwort 3: ");
      wifiInfo += wifiPassword3;
      wifiInfo += F("</li>\n");
    } else {
      wifiInfo += F("<li>Name des WLANs: ");
      wifiInfo += String(wifiApSsid);
      wifiInfo += F("</li>\n<li>Passwort: ");
      wifiInfo += wifiApPasswortAktiviert ? String(wifiApPasswort) : F("WLAN ohne Passwortschutz!");
      wifiInfo += F("</li>\n");
    }
    sendeDebugInfo(F("Wifi Modul"), wifiInfo);
  #endif

  #if MODUL_WEBHOOK
    String webhookInfo;
    webhookInfo += F("<li>Webhook Alarm angeschalten?: ");
    webhookInfo += String(webhookAn);
    webhookInfo += F("<li>Webhook Domain: ");
    webhookInfo += webhookDomain;
    webhookInfo += F("<li>Webhook URL: ");
    webhookInfo += webhookPfad;
    webhookInfo += F("</li>\n");
    sendeDebugInfo(F("Webhook Modul"), webhookInfo);
  #endif

  #if MODUL_ANALOG3
    sendeAnalogsensorDebugInfo(3, analog3Name, analog3Messwert, analog3MesswertProzent, analog3Minimum, analog3Maximum, analog3Webhook);
  #endif
  #if MODUL_ANALOG4
    sendeAnalogsensorDebugInfo(4, analog4Name, analog4Messwert, analog4MesswertProzent, analog4Minimum, analog4Maximum, analog3Webhook);
  #endif
  #if MODUL_ANALOG5
    sendeAnalogsensorDebugInfo(5, analog5Name, analog5Messwert, analog5MesswertProzent, analog5Minimum, analog5Maximum, analog3Webhook);
  #endif
  #if MODUL_ANALOG6
    sendeAnalogsensorDebugInfo(6, analog6Name, analog6Messwert, analog6MesswertProzent, analog6Minimum, analog6Maximum, analog3Webhook;
  #endif
  #if MODUL_ANALOG7
    sendeAnalogsensorDebugInfo(7, analog7Name, analog7Messwert, analog7MesswertProzent, analog7Minimum, analog7Maximum, analog3Webhook);
  #endif
  #if MODUL_ANALOG8
    sendeAnalogsensorDebugInfo(8, analog8Name, analog8Messwert, analog8MesswertProzent, analog8Minimum, analog8Maximum, analog3Webhook);
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
}
