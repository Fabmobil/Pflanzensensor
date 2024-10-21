/**
 * @file Pflanzensensor.ino
 * @brief Hauptprogramm des Fabmobil Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Hauptfunktionen setup() und loop() sowie
 * weitere zentrale Funktionen für den Betrieb des Pflanzensensors.
 */

#include "einstellungen.h" // Alle Einstellungen werden dort vorgenommen!
#include "passwoerter.h" // hier liegen die Passwoerter

#if MODUL_DHT
    #include "dht.h"
#endif

#if MODUL_LEDAMPEL
    #include "ledampel.h"
#endif

#if MODUL_MULTIPLEXER
    #include "multiplexer.h"
#endif

#if MODUL_DISPLAY
    #include "display.h"
#endif

#if MODUL_WIFI
    #include "wifi.h"
#endif

#if MODUL_WEBHOOK
    #include "webhook.h"
#endif

/**
 * @brief Initialisierungsfunktion, die einmalig beim Start des Mikrocontrollers ausgeführt wird
 *
 * Diese Funktion initialisiert alle Sensoren, Module und Verbindungen des Pflanzensensors.
 * Sie wird automatisch aufgerufen, wenn der Mikrocontroller gestartet oder zurückgesetzt wird.
 *
 * Folgende Aktionen werden ausgeführt:
 * - Serielle Verbindung wird aufgebaut
 * - Display wird initialisiert (wenn aktiviert)
 * - Mutex wird erstellt
 * - LED-Ampel wird initialisiert (wenn aktiviert)
 * - Analoger Eingang wird konfiguriert
 * - WiFi-Verbindung wird hergestellt (wenn aktiviert)
 * - DHT-Sensor wird initialisiert (wenn aktiviert)
 * - Variablen werden geladen oder gespeichert
 * - Webhook wird eingerichtet (wenn aktiviert)
 */
void setup() {
  Serial.begin(baudrateSeriell); // Serielle Verbindung aufbauen
  logger.SetzteLogLevel(LogLevel::INFO); // oder ein anderes gewünschtes Log-Level
  logger.NTPInitialisieren();
  delay(100);
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    DisplaySetup(); // Display initialisieren
  #endif
  CreateMutex(&mutex);
  logger.debug(F("Start von setup()"));

  logger.info(F(" Fabmobil Pflanzensensor, v") + String(pflanzensensorVersion));
  module = ModuleZaehlen(); // wie viele Module sind aktiv?

  #if MODUL_LEDAMPEL // wenn das LED Ampel Modul aktiv is:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" Ampel-"), F("  modul"));
    #endif
    logger.info(F("Start von Ledampel-Modul ... "));
    pinMode(ampelPinGruen, OUTPUT); // LED 1 (grün)
    pinMode(ampelPinGelb, OUTPUT); // LED 2 (gelb)
    pinMode(ampelPinRot, OUTPUT); // LED 3 (rot)
    // alle LEDs blinken beim Start als Funktionstest:
    LedampelBlinken("gruen", 1, 300);
    LedampelBlinken("gelb", 1, 300);
    LedampelBlinken("rot", 1, 300);

    logger.debug(F("Setup der Ledampel"));
  #endif
  #if MODUL_HELLIGKEIT || MODUL_BODENFEUCHTE // "||" ist ein logisches Oder: Wenn Helligkeits- oder Bodenfeuchtemodul aktiv ist
    pinMode(pinAnalog, INPUT);  // wird der Analogpin als Eingang gesetzt
  #endif
  #if MODUL_MULTIPLEXER // wenn das Multiplexer Modul aktiv ist werden die drei Multiplexerpins als Ausgang gesetzt:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" Multiplexer-"), F("  modul"));
    #endif
    logger.info(F("Start von Multiplexer-Modul ... "));
    pinMode(multiplexerPinA, OUTPUT); // Pin A des Multiplexers
    pinMode(multiplexerPinB, OUTPUT); // Pin B des Multiplexers
    pinMode(multiplexerPinC, OUTPUT); // Pin C des Multiplexers
  #else
    pinMode(16, OUTPUT); // interne LED auf D0 / GPIO16
    digitalWrite(16, HIGH); // wird ausgeschalten (invertiertes Verhalten!)
  #endif
  if (!LittleFS.begin()) {  // Dateisystem initialisieren, muss vor Wifi geschehen
    logger.error(F("Fehler: LittleFS konnte nicht initialisiert werden!"));
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" LittleFS"), F("  Fehler!"));
    #endif
    return;
  }
  #if MODUL_WIFI
    #if MODUL_DISPLAY
      DisplayDreiWoerter(F("Start.."), F(" Wifi-"), F("  modul"));
    #endif
    logger.info(F("Start von Wifi-Modul ... "));
    String ip = WifiSetup(wifiHostname); // Wifi-Verbindung herstellen und IP Adresse speichern
    logger.info("wifiPasswort2: " + wifiPasswort2);
    if (ip == "keine WLAN Verbindung.") {
      logger.warning(F("Keine WLAN-Verbindung möglich. Wechsel in den Accesspoint-Modus."));
      #if MODUL_DISPLAY
        DisplayDreiWoerter(F("Kein WLAN"), F("Starte"), F("Accesspoint"));
      #endif
      wifiAp = true;
      String ip = WifiSetup(wifiHostname);
    }
  #endif
  #if MODUL_DHT // wenn das DHT Modul aktiv ist:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" DHT-"), F("  modul"));
    #endif
    logger.info(F("Start von DHT-Modul ... "));
    // Initialisierung des Lufttemperatur und -feuchte Sensors:
    dht.begin(); // Sensor initialisieren
  #endif
  #if MODUL_MULTIPLEXER
    digitalWrite(multiplexerPinB, HIGH); // eingebaute LED ausschalten
    digitalWrite(multiplexerPinC, HIGH); // eingebaute LED ausschalten
  #endif
  if (VariablenDa()) {
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" Variablen"), F("  laden"));
    #endif
    // Load the preferences from flash
    VariablenLaden();
  } else {
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" Variablen"), F("  speichern"));
    #endif
    // Save the preferences to flash
    VariablenSpeichern();
  }

  variablen.begin("pflanzensensor", false); // Variablen initialisieren
  neustarts = variablen.getInt("neustarts"); // Anzahl der Neustarts auslesen

  neustarts++;
  variablen.putInt("neustarts", neustarts);
  variablen.end();
  logger.info(F("Neustarts: ") + String(neustarts));
  #if MODUL_WEBHOOK
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter(F("Start.."), F(" Webhook-"), F("  modul"));
    #endif
    logger.info(F("Start von Webhook-Modul ... "));
    WebhookSetup();
  #endif
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    DisplayDreiWoerter(F("Start.."), F(" abge-"), F(" schlossen"));
  #endif
  logger.info(F("Start abgeschlossen!"));
}

/**
 * @brief Hauptschleifenfunktion, die kontinuierlich ausgeführt wird
 *
 * Diese Funktion bildet die Hauptschleife des Programms und wird ständig wiederholt.
 * Sie führt regelmäßige Messungen und Aktualisierungen durch, basierend auf festgelegten Intervallen.
 *
 * Folgende Aktionen werden in jedem Durchlauf überprüft und bei Bedarf ausgeführt:
 * - Messung aller Analogsensoren
 * - Messung von Luftfeuchtigkeit und -temperatur (wenn DHT-Modul aktiviert)
 * - Aktualisierung der LED-Ampel (wenn aktiviert)
 * - Aktualisierung der Displayanzeige (wenn aktiviert)
 * - Verarbeitung von WiFi- und Webserver-Anfragen (wenn aktiviert)
 * - Senden von Webhook-Benachrichtigungen (wenn aktiviert)
 */
void loop() {
  /*
   * millis() ist eine Funktion, die die Millisekunden seit dem Start des Programms ausliest.
   * sie wird hier anstatt von delay() verwendet, weil sie den Programmablauf nicht blockiert.
   * Das ist wichtig, damit der Webserver schnell genug auf Anfragen reagieren kann.
   * Alle Sensoren werden nach einem definierten Intervall, welches mit dem Millis-Wert verglichen wird,
   * ausgelesen. Dazwischen werden nur Anfragen an den Webserver abgefragt.
   */
  millisAktuell = millis(); // aktuelle Millisekunden auslesen

  logger.NTPUpdaten(); // Update Timestamp

  // Alle Analogsensoren werden hintereinander gemessen
  if (millisAktuell - millisVorherAnalog >= intervallAnalog) { // wenn das Intervall erreicht ist
    if (GetMutex(&mutex)) {
      millisVorherAnalog = millisAktuell; // neuen Wert übernehmen

      logger.debug(F("intervallAnalog erreicht."));

      // Helligkeit messen:
      #if MODUL_HELLIGKEIT  // wenn das Helligkeit Modul aktiv ist
        std::tie(helligkeitMesswert, helligkeitMesswertProzent) =
          AnalogsensorMessen(1,1,1, helligkeitName, helligkeitMinimum, helligkeitMaximum);
        helligkeitFarbe = FarbeBerechnen(helligkeitMesswertProzent, helligkeitGruenUnten, helligkeitGruenOben, helligkeitGelbUnten, helligkeitGelbOben);
      #endif

      // Bodenfeuchte messen:
      #if MODUL_BODENFEUCHTE // wenn das Bodenfeuchte Modul aktiv is
        std::tie(bodenfeuchteMesswert, bodenfeuchteMesswertProzent) =
          AnalogsensorMessen(0,1,1, bodenfeuchteName, bodenfeuchteMinimum, bodenfeuchteMaximum);
        bodenfeuchteFarbe = FarbeBerechnen(bodenfeuchteMesswertProzent, bodenfeuchteGruenUnten, bodenfeuchteGruenOben, bodenfeuchteGelbUnten, bodenfeuchteGelbOben);
      #endif

      // Analogsensor3 messen:
      #if MODUL_ANALOG3 // wenn das Analog3 Modul aktiv ist
        std::tie(analog3Messwert, analog3MesswertProzent) =
          AnalogsensorMessen(1,0,1, analog3Name, analog3Minimum, analog3Maximum);
        analog3Farbe = FarbeBerechnen(analog3MesswertProzent, analog3GruenUnten, analog3GruenOben, analog3GelbUnten, analog3GelbOben);
      #endif

      // Analogsensor4 messen:
      #if MODUL_ANALOG4 // wenn das Analog4 Modul aktiv ist
        std::tie(analog4Messwert, analog4MesswertProzent) =
          AnalogsensorMessen(0,0,1, analog4Name, analog4Minimum, analog4Maximum);
        analog4Farbe = FarbeBerechnen(analog4MesswertProzent, analog4GruenUnten, analog4GruenOben, analog4GelbUnten, analog4GelbOben);
      #endif

      // Analogsensor5 messen:
      #if MODUL_ANALOG5 // wenn das Analog5 Modul aktiv ist
      std::tie(analog5Messwert, analog5MesswertProzent) =
          AnalogsensorMessen(1,1,0, analog5Name, analog5Minimum, analog5Maximum);
        analog5Farbe = FarbeBerechnen(analog5MesswertProzent, analog5GruenUnten, analog5GruenOben, analog5GelbUnten, analog5GelbOben);
      #endif

      // Analogsensor6 messen:
      #if MODUL_ANALOG6 // wenn das Analog6 Modul aktiv ist
        std::tie(analog6Messwert, analog6MesswertProzent) =
          AnalogsensorMessen(0,1,0, analog6Name, analog6Minimum, analog6Maximum);
        analog6Farbe = FarbeBerechnen(analog6MesswertProzent, analog6GruenUnten, analog6GruenOben, analog6GelbUnten, analog6GelbOben);
      #endif

      // Analogsensor7 messen:
      #if MODUL_ANALOG7 // wenn das Analog7 Modul aktiv ist
        std::tie(analog7Messwert, analog7MesswertProzent) =
          AnalogsensorMessen(1,0,0, analog7Name, analog7Minimum, analog7Maximum);
        analog7Farbe = FarbeBerechnen(analog7MesswertProzent, analog7GruenUnten, analog7GruenOben, analog7GelbUnten, analog7GelbOben);
      #endif

      // Analogsensor8 messen:
      #if MODUL_ANALOG8 // wenn das Analog8 Modul aktiv ist
        std::tie(analog8Messwert, analog8MesswertProzent) =
          AnalogsensorMessen(0,0,0, analog8Name, analog8Minimum, analog8Maximum);
        analog8Farbe = FarbeBerechnen(analog8MesswertProzent, analog8GruenUnten, analog8GruenOben, analog8GelbUnten, analog8GelbOben);
      #endif
      #if MODUL_MULTIPLEXER
        digitalWrite(multiplexerPinB, HIGH); // eingebaute LED ausschalten
        digitalWrite(multiplexerPinC, HIGH); // eingebaute LED ausschalten
      #endif
      ReleaseMutex(&mutex);
    }
  }

  // Luftfeuchte und -temperatur messen:
  #if MODUL_DHT // wenn das DHT Modul aktiv ist
    if (millisAktuell - millisVorherDht >= intervallDht) { // wenn das Intervall erreicht ist

      logger.debug(F("intervallDht erreicht."));

      millisVorherDht = millisAktuell; // neuen Wert übernehmen
      lufttemperaturMesswert = MesseLufttemperatur(); // Lufttemperatur messen
      lufttemperaturFarbe = FarbeBerechnen(lufttemperaturMesswert, lufttemperaturGruenUnten, lufttemperaturGruenOben, lufttemperaturGelbUnten, lufttemperaturGelbOben);
      luftfeuchteMesswert = MesseLuftfeuchtigkeit(); // Luftfeuchte messen
      luftfeuchteFarbe = FarbeBerechnen(luftfeuchteMesswert, luftfeuchteGruenUnten, luftfeuchteGruenOben, luftfeuchteGelbUnten, luftfeuchteGelbOben);
    }
  #endif

  // LED Ampel Modus 0: Anzeige der Bodenfeuchte
  #if MODUL_LEDAMPEL // Wenn das LED Ampel Modul aktiv ist:
    if (ampelAn && ampelModus == 0) {
      LedampelAnzeigen(bodenfeuchteFarbe, -1);
    } else if (!ampelAn) {
      LedampelAus();
    }
  #endif

  // Messwerte auf dem Display anzeigen:

  #if MODUL_DISPLAY
    if (displayAn) {
      if (millisAktuell - millisVorherDisplay >= intervallDisplay) {
        millisVorherDisplay = millisAktuell;
        DisplayAnzeigen();
        NaechsteSeite();
        logger.info(F("IP Adresse: ") + String(ip));
      }
    }
  #endif


  // Wifi und Webserver:
  #if MODUL_WIFI // wenn das Wifi-Modul aktiv ist
    if (GetMutex(&mutex)) { // Mutex holen
      if (wlanNeustartGeplant && millis() >= geplanteWLANNeustartZeit) {
        wlanNeustartGeplant = false;
        NeustartWLANVerbindung(); // Führt den tatsächlichen Neustart durch
      }
      // WLAN Verbindung aufrecht erhalten:
      // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiMulti/WiFiMulti.ino
      if (!wifiAp) {
        if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
          ip = WiFi.localIP().toString(); // IP Adresse in Variable schreiben
          aktuelleSsid = WiFi.SSID(); // SSID in Variable schreiben
          wifiVerbindungsVersuche = 0; // Zurücksetzen des Zählers bei erfolgreicher Verbindung
        } else {
          wifiVerbindungsVersuche++; // Erhöhen des Zählers bei fehlgeschlagener Verbindung
          if (wifiVerbindungsVersuche >= 10) {
            logger.warning(F("Fehler: WLAN Verbindung verloren! Wechsle in den Accesspoint-Modus."));
            #if MODUL_DISPLAY
              DisplayDreiWoerter(F("WLAN"), F("Verbindung"), F("verloren!"));
            #endif
            wifiAp = true;
            String ip = WifiSetup(wifiHostname);
            aktuelleSsid = wifiApSsid; // AP SSID in Variable schreiben
            wifiVerbindungsVersuche = 0; // Zurücksetzen des Zählers
          } else {
            logger.info(F("WLAN-Verbindungsversuch fehlgeschlagen. Versuch ") + String(wifiVerbindungsVersuche) + F(" von 10"));
          }
        }
      }
      Webserver.handleClient(); // der Webserver soll in jedem loop nach Anfragen schauen!
      ReleaseMutex(&mutex); // Mutex wieder freigeben
    }
  #endif

  // Webhook für Alarm:
 #if MODUL_WEBHOOK
  if (webhookAn) {
      unsigned long aktuelleZeit = millis();
      bool aktuellerAlarmStatus = WebhookAktualisiereAlarmStatus();
      String neuerStatus = aktuellerAlarmStatus ? "Alarm" : "OK";

      // Überprüfe, ob es Zeit für eine reguläre Übertragung ist (Alarm oder OK)
      bool sendeAlarm = (aktuelleZeit - millisVorherWebhook >= (unsigned long)(webhookFrequenz) * 1000UL * 60UL * 60UL);

      // Überprüfe, ob es Zeit für einen Ping ist
      bool sendePing = (aktuelleZeit - millisVorherWebhookPing >= (unsigned long)(webhookPingFrequenz) * 1000UL * 60UL * 60UL);

      if (sendePing) {
          WebhookErfasseSensordaten("ping");
          millisVorherWebhookPing = aktuelleZeit;
          letzterWebhookStatus = neuerStatus;
      } else if (sendeAlarm && (neuerStatus == "Alarm" || (neuerStatus == "OK" && letzterWebhookStatus == "Alarm"))) {
          WebhookErfasseSensordaten("normal");
          millisVorherWebhook = aktuelleZeit;
          letzterWebhookStatus = neuerStatus;
      }

      vorherAlarm = aktuellerAlarmStatus;
  }
  #endif

}

/**
 * @brief Zählt die Anzahl der aktiven Module
 *
 * Diese Funktion überprüft, welche Module aktiviert sind und zählt sie.
 * Sie wird verwendet, um einen Überblick über die aktuelle Konfiguration zu erhalten.
 *
 * @return int Anzahl der aktiven Module
 */
int ModuleZaehlen() {
    int aktiveModule = 0;
    if (MODUL_BODENFEUCHTE) aktiveModule++; // wenn das Bodenfeuchte Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_DISPLAY) aktiveModule++; // wenn das Display Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_DHT) aktiveModule++; // wenn das DHT Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_HELLIGKEIT) aktiveModule++; // wenn das Helligkeit Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_WEBHOOK) aktiveModule++; // wenn das IFTTT Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_LEDAMPEL) aktiveModule++; // wenn das LED Ampel Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_WIFI) aktiveModule++; // wenn das Wifi Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_ANALOG3) aktiveModule++; // wenn das Analog3 Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_ANALOG4) aktiveModule++; // wenn das Analog4 Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_ANALOG5) aktiveModule++; // wenn das Analog5 Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_ANALOG6) aktiveModule++; // wenn das Analog6 Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_ANALOG7) aktiveModule++; // wenn das Analog7 Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_ANALOG8) aktiveModule++; // wenn das Analog8 Modul aktiv ist, wird die Variable um 1 erhöht
    return aktiveModule; // die Anzahl der aktiven Module wird zurückgegeben
}

/**
 * @brief Berechnet die Ampelfarbe basierend auf einem Messwert und definierten Schwellwerten
 *
 * Diese Funktion ermittelt, ob ein gegebener Messwert im grünen, gelben oder roten Bereich liegt.
 * Sie wird verwendet, um den Zustand der LED-Ampel und die Farbcodierung auf dem Display zu bestimmen.
 *
 * @param messwert Der zu überprüfende Messwert
 * @param gruenUnten Unterer Grenzwert für den grünen Bereich
 * @param gruenOben Oberer Grenzwert für den grünen Bereich
 * @param gelbUnten Unterer Grenzwert für den gelben Bereich
 * @param gelbOben Oberer Grenzwert für den gelben Bereich
 * @return String "gruen", "gelb" oder "rot", je nach Einordnung des Messwerts
 */
String FarbeBerechnen(int messwert, int gruenUnten, int gruenOben, int gelbUnten, int gelbOben) {
  if (messwert >= gruenUnten && messwert <= gruenOben) {
    return "gruen";
  } else if (messwert < gelbUnten || messwert > gelbOben) {
    return "rot";
  } else {
    return "gelb";
  }
}

