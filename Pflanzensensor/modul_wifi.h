/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul und den Webserver
 */

#include <ESP8266WiFi.h> // für WLAN
#include <ESP8266WebServer.h> // für Webserver
#include <ESP8266mDNS.h> // für Namensauflösung

WiFiClient client;
ESP8266WebServer Webserver(80); //

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
    formatierterCode += "<h2>Helligkeit</h2><p>";
    formatierterCode += messwertHelligkeitProzent;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_BODENFEUCHTE
    formatierterCode += "<h2>Bodenfeuchte</h2><p>";
    formatierterCode += messwertBodenfeuchteProzent;
    formatierterCode += "%</p>";
  #endif
  #if MODUL_DHT
    formatierterCode += "<h2>Lufttemperatur</h2><p>";
    formatierterCode += messwertLufttemperatur;
    formatierterCode += "°C</p>";
    formatierterCode += "<h2>Luftfeuchte</h2><p>";
    formatierterCode += messwertLuftfeuchte;
    formatierterCode += "%</p>";
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

  formatierterCode += "<h3>DHT Modul</h3>";
  #if MODUL_DHT
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
  #else
    formatierterCode += "<p>DHT Modul deaktiviert!</p>";
  #endif

  formatierterCode += "<h3>Display Modul</h3>";
  #if MODUL_DISPLAY
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
  #else
    formatierterCode += "<p>Display Modul deaktiviert!</p>";
  #endif

  formatierterCode += "<h3>Bodenfeuchte Modul</h3>";
  #if MODUL_BODENFEUCHTE
    formatierterCode += "<ul>";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += messwertBodenfeuchteProzent;
    formatierterCode += "</li>";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += messwertBodenfeuchte;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #else
    formatierterCode += "<p>Bodenfeuchte Modul deaktiviert!</p>";
  #endif

  formatierterCode += "<h3>LEDAmpel Modul</h3>";
  #if MODUL_LEDAMPEL
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
  #else
    formatierterCode += "<p>Bodenfeuchte Modul deaktiviert!</p>";
  #endif

  formatierterCode += "<h3>Helligkeit Modul</h3>";
  #if MODUL_HELLIGKEIT
    formatierterCode += "<ul>";
    formatierterCode += "<li>Messwert Prozent: ";
    formatierterCode += messwertHelligkeitProzent;
    formatierterCode += "</li>";
    formatierterCode += "<li>Messwert absolut: ";
    formatierterCode += messwertHelligkeit;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #else
    formatierterCode += "<p>Helligkeit Modul deaktiviert!</p>";
  #endif

  formatierterCode += "<h3>Wifi Modul</h3>";
  #if MODUL_WIFI
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
  #else
    formatierterCode += "<p>Wifi Modul deaktiviert!</p>";
  #endif
  formatierterCode += "<h3>IFTTT Modul</h3>";
  #if MODUL_IFTTT
    formatierterCode += "<ul>";
    formatierterCode += "<li>IFTTT Passwort: ";
    formatierterCode += wifiIftttPasswort;
    formatierterCode += "</li>";
    formatierterCode += "<li>IFTTT Ereignis: ";
    formatierterCode += wifiIftttEreignis;
    formatierterCode += "</li>";
    formatierterCode += "</ul>";
  #else
    formatierterCode += "<p>IFTTT Modul deaktiviert!</p>";
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
  #if MODUL_DISPLAY
    formatierterCode += "<h2>Display</h2>";
    formatierterCode += "<p>status (Anzeigenummer auf dem Display):";
    formatierterCode += "<input type=\"text\" size=\"4\" name=\"statis\" placeholder=\"";
    formatierterCode += status;
    formatierterCode += "\"></p>";
  #endif
  #if MODUL_HELLIGKEIT
    formatierterCode += "<h2>Helligkeitssensor</h2>";
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
  formatierterCode += "<h2>Passwort</h2>";
  formatierterCode += "<p><input type=\"password\" name=\"Passwort\" placeholder=\"Passwort\"><br>";
  formatierterCode += "<input type=\"submit\" value=\"Absenden\"></p></form>";

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
 * Übernimmt die Änderungen, welche auf der Administrationsseite gemacht wurden.
 */
void WebseiteSetzeVariablen() {
  #include "modul_wifi_bilder.h" // Bilder die auf der Seite verwendet werden
  #include "modul_wifi_header.h" // Kopf der HTML-Seite
  #include "modul_wifi_footer.h" // Fuß der HTML-Seite
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebseiteSetzeVariablen()"));
  #endif
  if ( ! Webserver.hasArg("Passwort") || Webserver.arg("Passwort") == NULL) { // wenn kein Passwort übergeben wurde
    Webserver.send(400, "text/plain", "400: Invalid Request"); // Fehlermeldung ausgeben
    return;
  }
  if ( Webserver.arg("Passwort") == wifiAdminPasswort) { // wenn das Passwort stimmt
      if ( Webserver.arg("ampelModus") != "" ) { // wenn ein neuer Wert für ampelModus übergeben wurde
        ampelModus = Webserver.arg("ampelModus").toInt(); // neuen Wert übernehmen
      }
      #if MODUL_HELLIGKEIT
        if ( Webserver.arg("ampelHelligkeitGruen") != "" ) { // wenn ein neuer Wert für ampelHelligkeitGruen übergeben wurde
          ampelHelligkeitGruen = Webserver.arg("ampelHelligkeitGruen").toInt(); // neuen Wert übernehmen
        }
        if ( Webserver.arg("ampelHelligkeitRot") != "" ) { // wenn ein neuer Wert für ampelHelligkeitRot übergeben wurde
          ampelHelligkeitRot = Webserver.arg("ampelHelligkeitRot").toInt(); // neuen Wert übernehmen
        }
      #endif
      #if MODUL_BODENFEUCHTE
        if ( Webserver.arg("ampelBodenfeuchteGruen") != "" ) { // wenn ein neuer Wert für ampelBodenfeuchteGruen übergeben wurde
          ampelBodenfeuchteGruen = Webserver.arg("ampelBodenfeuchteGruen").toInt(); // neuen Wert übernehmen
        }
        if ( Webserver.arg("ampelBodenfeuchteRot") != "" ) { // wenn ein neuer Wert für ampelBodenfeuchteRot übergeben wurde
          ampelBodenfeuchteRot = Webserver.arg("ampelBodenfeuchteRot").toInt(); // neuen Wert übernehmen
        }
      #endif
    #endif
    #if MODUL_DISPLAY
      if ( Webserver.arg("status") != "" ) { // wenn ein neuer Wert für status übergeben wurde
          status = Webserver.arg("status").toInt(); // neuen Wert übernehmen
        }
    #endif
    #if MODUL_HELLIGKEIT
      if ( Webserver.arg("helligkeitMinimum") != "" ) { // wenn ein neuer Wert für helligkeitMinimum übergeben wurde
        helligkeitMinimum = Webserver.arg("helligkeitMinimum").toInt(); // neuen Wert übernehmen
      }
      if ( Webserver.arg("helligkeitMaximum") != "" ) { // wenn ein neuer Wert für helligkeitMaximum übergeben wurde
        helligkeitMaximum = Webserver.arg("helligkeitMaximum").toInt(); // neuen Wert übernehmen
      }
    #endif
    String formatierterCode = htmlHeader; // formatierterCode beginnt mit Kopf der HTML-Seite
    formatierterCode += "<h2>Erfolgreich!</h2>";
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
  } else { // wenn das Passwort falsch ist
    String formatierterCode = htmlHeader; // formatierterCode beginnt mit Kopf der HTML-Seite
    formatierterCode += "<h2>Falsches Passwort!</h2>";
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
    formatierterCode += htmlFooter; // formatierterCode endet mit Fuß der HTML-Seite
  }
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
