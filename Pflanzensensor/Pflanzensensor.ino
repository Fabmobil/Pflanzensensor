/*
 * FABMOBIL Pflanzensensor
 * ***********************
 * Author: tommy@fabmobil.org
 *
 * Dies ist der Code für den Fabmobil Pflanzensensor. Er ist aufgeteilt in verschiedene Dateien:
 * - Pflanzensensor.ino ist die zentrale Datei, welche die setup() und loop()-Funktion enthält,
 *   die für den Betrieb des Sensors notwendig ist.
 * - Configuration.h enthält Konfigurationsdefinitionen und ermöglicht es, Module an- oder aus-
 *   zuschalten, die Pins der Sensoren zu definieren und verschiedene Variablen zu setzen
 * - modul_bodenfeuchte.h ist die Datei mit den Funktionen für den Bodenfeuchtesensor
 * - dht.h ist die Datei mit den Funktionen für den Lufttemperatur und -feuchtesensor
 * - display.h ist die Datei mit den Funktionen für das Display
 * - ledampel.h ist die Datei mit den Funktionen für die LED Ampel
 * - MODUL_HELLIGKEIT.h ist die Datei mit den Funktionen für den Lichtsensor
 * - multiplexer.h ist die Datei mit den Funktionen für den Analogmultiplexer. Dieser
 *   kommt zum Einsatz wenn sowohl der Licht- als auch der Bodenfeuchtesensor eingesetzt
 *   werden, da beide Analogsignale liefern und der verwendete ESP8266 Chip nur einen
 *   Analogeingang hat.
 * - wifi.h ist die Datei mit den Funktionen für die Wifiverbindung
 *
 * Die verwendeten Bauteile sind in der Readme.md aufgeführt.
 */

#include "Configuration.h" // Alle Einstellungen werden dort vorgenommen!

/*
 * Funktion: setup()
 * Diese Funktion wird beim booten des Mikrocontrollers einmalig ausgeführt
 * und dient dazu, alle Sensoren etc. zu initialisieren
 */
void setup() {
  Serial.begin(baudrateSeriell); // Serielle Verbindung aufbauen
  /* Die #if ... #endif-Anweisungen werden vom Preprozessor aufgegriffen,
   * welcher den Code fürs kompilieren vorbereitet. Es wird abgefragt,
   * ob das jeweilige Modul aktiviert ist oder nicht. Dies geschieht in
   * der Configuration.h-Datei. Ist das Modul deaktiviert, wird der Code
   * zwischen dem #if und #endif ignoriert und landet nicht auf dem Chip.
   */
  #ifdef WITH_GDB // fürs debugging
    gdbstub_init();
  #endif
  delay(1000);
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    #if MODUL_DEBUG
      Serial.println(F("# Beginn von DisplaySetup()"));
    #endif
    DisplaySetup(); // Display initialisieren
  #endif
  CreateMutex(&mutex);
  #if MODUL_DEBUG
    Serial.println(F("#### Start von setup()"));
  #endif
  /* Serial.println() schreibt den Text in den Klammern als Ausgabe
   * auf die serielle Schnittstelle. Die kann z.B. in der Arduino-IDE
   * über Werkzeuge -> Serieller Monitor angeschaut werden so lange
   * eine USB-Verbindung zum Chip besteht. Der Unterschied zwischen
   * Serial.println() und Serial.print() ist, dass der erste Befehl
   * Das F("")-Makro innerhalb von Serial.print(); sorgt dafür, dass der Text
   * im Programmspeicher (flash) und nicht im Arbeitsspeicher (RAM) abgelegt
   * wird, dass funktioniert aber nicht mit Variablen.
   */
  Serial.println(" Fabmobil Pflanzensensor, v" + String(pflanzensensorVersion));
  module = ModuleZaehlen(); // wie viele Module sind aktiv?
  displayseiten = AnalogsensorenZaehlen() + 6;  // Wir haben 6 Displayseiten plus je eine pro Analogmodul

  #if MODUL_DEBUG // Debuginformationen
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter("Start..", " Debug-", "  modul");
    #endif
    Serial.println(F("Start von Debug-Modul ... "));
    Serial.print(F("# Anzahl Module: "));
    Serial.println(module);
    Serial.print(F("# Anzahl Displayseiten: "));
    Serial.println(displayseiten);
  #endif
  #if MODUL_LEDAMPEL // wenn das LED Ampel Modul aktiv is:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter("Start..", " Ampel-", "  modul");
    #endif
    Serial.println(F("Start von Ledampel-Modul ... "));
    pinMode(ampelPinGruen, OUTPUT); // LED 1 (grün)
    pinMode(ampelPinGelb, OUTPUT); // LED 2 (gelb)
    pinMode(ampelPinRot, OUTPUT); // LED 3 (rot)
    // alle LEDs blinken beim Start als Funktionstest:
    LedampelBlinken("gruen", 1, 300);
    LedampelBlinken("gelb", 1, 300);
    LedampelBlinken("rot", 1, 300);
    #if MODUL_DEBUG // Debuginformationen
      Serial.println(F("## Setup der Ledampel"));
      Serial.print(F("# PIN gruene LED:                 ")); Serial.println(ampelPinGruen);
      Serial.print(F("# PIN gelbe LED:                  ")); Serial.println(ampelPinGelb);
      Serial.print(F("# PIN rote LED:                   ")); Serial.println(ampelPinRot);
    #endif
  #endif
  #if MODUL_HELLIGKEIT || MODUL_BODENFEUCHTE // "||" ist ein logisches Oder: Wenn Helligkeits- oder Bodenfeuchtemodul aktiv ist
    pinMode(pinAnalog, INPUT);  // wird der Analogpin als Eingang gesetzt
  #endif
  #if MODUL_MULTIPLEXER // wenn das Multiplexer Modul aktiv ist werden die zwei Multiplexerpins als Ausgang gesetzt:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter("Start..", " Multiplexer-", "  modul");
    #endif
    Serial.println(F("Start von Multiplexer-Modul ... "));
    pinMode(multiplexerPinA, OUTPUT); // Pin A des Multiplexers
    pinMode(multiplexerPinB, OUTPUT); // Pin B des Multiplexers
    pinMode(multiplexerPinC, OUTPUT); // Pin C des Multiplexers
  #else
    pinMode(16, OUTPUT); // interne LED auf D0 / GPIO16
    digitalWrite(16, HIGH); // wird ausgeschalten (invertiertes Verhalten!)
  #endif
  if (!LittleFS.begin()) {  // Dateisystem initialisieren, muss vor Wifi geschehen
    Serial.println("Fehler: LittleFS konnte nicht initialisiert werden!");
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter("Start..", " LittleFS", "  Fehler!");
    #endif
    return;
  }
  #if MODUL_WIFI // wenn das Wifi Modul aktiv ist:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter("Start..", " Wifi-", "  modul");
    #endif
    Serial.println(F("Start von Wifi-Modul ... "));
    String ip = WifiSetup(wifiHostname); // Wifi-Verbindung herstellen und IP Adresse speichern
  #endif
  #if MODUL_DHT // wenn das DHT Modul aktiv ist:
    #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
      DisplayDreiWoerter("Start..", " DHT-", "  modul");
    #endif
    Serial.println(F("Start von DHT-Modul ... "));
    // Initialisierung des Lufttemperatur und -feuchte Sensors:
    dht.begin(); // Sensor initialisieren
  #endif
  #if MODUL_MULTIPLEXER
    digitalWrite(multiplexerPinB, HIGH); // eingebaute LED ausschalten
    digitalWrite(multiplexerPinC, HIGH); // eingebaute LED ausschalten
  #endif
  #if MODUL_WEBHOOK
    // Set time via NTP, as required for x.509 validation
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("");
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.print("Current time: ");
    Serial.print(asctime(&timeinfo));
  #endif
  if (VariablenDa()) {
      #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
        DisplayDreiWoerter("Start..", " Variablen", "  laden");
      #endif
      // Load the preferences from flash
      VariablenLaden();
    } else {
      #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
        DisplayDreiWoerter("Start..", " Variablen", "  speichern");
      #endif
      // Save the preferences to flash
      VariablenSpeichern();
    }

  variablen.begin("pflanzensensor", false); // Variablen initialisieren
  int neustarts = variablen.getInt("neustarts", 1); // default to 1
  #if MODUL_DEBUG
    Serial.print(F("# Reboot count: )"));
    Serial.println(neustarts);
    Serial.println("# Inhalte des Flashspeichers:");
    File root = LittleFS.open("/", "r");
    WebseiteDatenFlashAuflisten(root, 0);
  #endif
  neustarts++;
  variablen.end();

  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    DisplayDreiWoerter("Start..", " abge-", "  schlossen");
  #endif
  Serial.println(F("Start abgeschlossen!"));
}

/*
 * Funktion: loop()
 * Zentrale Schleifenfunktion die immer wieder neu ausgeführt wird wenn sie abgeschlossen ist.
 */
void loop() {
  /*
   * millis() ist eine Funktion, die die Millisekunden seit dem Start des Programms ausliest.
   * sie wird hier anstatt von delay() verwendet, weil sie den Programmablauf nicht blockiert.
   * Das ist wichtig, damit der Webserver schnell genug auf Anfragen reagieren kann.
   * Alle Sensoren werden nach einem definierten Intervall, welches mit dem Millis-Wert verglichen wird,
   * ausgelesen. Dazwischen werden nur Anfragen an den Webserver abgefragt.
   */
  unsigned long millisAktuell = millis(); // aktuelle Millisekunden auslesen
  #if MODUL_DEBUG // Debuginformation
    Serial.println(F("############ Begin von loop() #############"));
    #if MODUL_DISPLAY
      Serial.print(F("# status: "));
      Serial.print(status);
    #endif
    Serial.print(F(", millis: "));
    Serial.println(millisAktuell);
    Serial.print(F("# IP Adresse: "));
    if ( wifiAp ) { // Falls der ESP sein eigenes WLAN aufgemacht hat:
      Serial.println(WiFi.softAPIP());
      Serial.print(F("# Anzahl der mit dem Accesspoint verbundenen Geräte: "));
      Serial.println(WiFi.softAPgetStationNum());
    } else {
      Serial.println(WiFi.localIP());
    }
    delay(2000); // Programmablauf verlangsamen um Debuginformationen lesen zu können
  #endif

  MDNS.update(); // MDNS updaten

  // Alle Analogsensoren werden hintereinander gemessen
  if (millisAktuell - millisVorherAnalog >= intervallAnalog) { // wenn das Intervall erreicht ist
    if (GetMutex(&mutex)) {
      millisVorherAnalog = millisAktuell; // neuen Wert übernehmen
      #if MODUL_DEBUG
        Serial.println(F("### intervallAnalog erreicht."));
      #endif
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
      #if MODUL_DEBUG
        Serial.println(F("### intervallDht erreicht."));
      #endif
      millisVorherDht = millisAktuell; // neuen Wert übernehmen
      lufttemperaturMesswert = DhtMessenLufttemperatur(); // Lufttemperatur messen
      lufttemperaturFarbe = FarbeBerechnen(lufttemperaturMesswert, lufttemperaturGruenUnten, lufttemperaturGruenOben, lufttemperaturGelbUnten, lufttemperaturGelbOben);
      luftfeuchteMesswert = DhtMessenLuftfeuchte(); // Luftfeuchte messen
      luftfeuchteFarbe = FarbeBerechnen(luftfeuchteMesswert, luftfeuchteGruenUnten, luftfeuchteGruenOben, luftfeuchteGelbUnten, luftfeuchteGelbOben);
    }
  #endif

  // LED Ampel umschalten:
  #if MODUL_LEDAMPEL // Wenn das LED Ampel Modul aktiv ist:
    if (millisAktuell - millisVorherLedampel >= intervallAmpel) { // wenn das Intervall erreicht ist
      #if MODUL_DEBUG // Debuginformation
        Serial.println(F("### intervallLedAmpel erreicht."));
      #endif
      millisVorherLedampel = millisAktuell; // neuen Wert übernehmen
      LedampelUmschalten(helligkeitMesswertProzent, bodenfeuchteMesswertProzent); // Ampel umschalten
    }
  #endif

  // Messwerte auf dem Display anzeigen:
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist
    if (millisAktuell - millisVorherDisplay >= intervallDisplay) {
      status += 1; // status gibt an, welche Anzeige auf dem Display aktiv ist
      if ( status == displayseiten) { // wenn wir das letzte Displaybild erreicht haben
        status = 0; // danach geht es von neuem los
      }
      #if MODUL_DEBUG
        Serial.print(F("### intervallDisplay erreicht. status: ")); Serial.println(status);
      #endif
      millisVorherDisplay = millisAktuell; // neuen Wert übernehmen
      // Diese Funktion kümmert sich um die Displayanzeige:
      DisplayAnzeigen();
    }
  #endif


  // Wifi und Webserver:
  #if MODUL_WIFI // wenn das Wifi-Modul aktiv ist
    if (GetMutex(&mutex)) { // Mutex holen
      // WLAN Verbindung aufrecht erhalten:
      // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/WiFiMulti/WiFiMulti.ino
        if (!wifiAp) {
          if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
            ip = WiFi.localIP().toString(); // IP Adresse in Variable schreiben
          } else {
            Serial.println("Fehler: WLAN Verbindung verloren!");
            #if MODUL_DISPLAY
              DisplayDreiWoerter("WLAN", "Verbindung", "verloren!");
            #endif
          }
        }
      Webserver.handleClient(); // der Webserver soll in jedem loop nach Anfragen schauen!
      ReleaseMutex(&mutex); // Mutex wieder freigeben
    }
  #endif

  // Webhook für Alarm:
  #if MODUL_WEBHOOK // wenn das Webhook-Modul aktiv ist
    if (webhookSchalter) {
      Serial.println("Webhook ausgelöst!");
      WebhookNachricht(bodenfeuchteMesswertProzent, luftfeuchteMesswert, lufttemperaturMesswert);
    }
    webhookSchalter = false;
  #endif

  #if MODUL_DEBUG // Debuginformation
    Serial.print(F("# millisAktuell: ")); Serial.println(millisAktuell);
    Serial.println(F("############ Ende von loop() ##############"));
    Serial.println(F(""));
  #endif
}

/* Funktion: ModuleZaehlen()
 * Funktion, um die Anzahl der aktiven Module zu zählen
 * gibt die Anzahl der Module als Integer zurück.
 */
int ModuleZaehlen() {
    int aktiveModule = 0;
    if (MODUL_BODENFEUCHTE) aktiveModule++; // wenn das Bodenfeuchte Modul aktiv ist, wird die Variable um 1 erhöht
    if (MODUL_DEBUG) aktiveModule++; // wenn das Debug Modul aktiv ist, wird die Variable um 1 erhöht
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

/* Funktion: AnalogsensorenZaehlen()
 * Funktion, um die Anzahl der aktiven Analogsensormodule zu zählen
* gibt die Anzahl der Module als Integer zurück.
*/
int AnalogsensorenZaehlen() {
  int analogsensoren = 0;
  if (MODUL_ANALOG3) analogsensoren++; // wenn das Analog3 Modul aktiv ist, wird die Variable um 1 erhöht
  if (MODUL_ANALOG4) analogsensoren++; // wenn das Analog4 Modul aktiv ist, wird die Variable um 1 erhöht
  if (MODUL_ANALOG5) analogsensoren++; // wenn das Analog5 Modul aktiv ist, wird die Variable um 1 erhöht
  if (MODUL_ANALOG6) analogsensoren++; // wenn das Analog6 Modul aktiv ist, wird die Variable um 1 erhöht
  if (MODUL_ANALOG7) analogsensoren++; // wenn das Analog7 Modul aktiv ist, wird die Variable um 1 erhöht
  if (MODUL_ANALOG8) analogsensoren++; // wenn das Analog8 Modul aktiv ist, wird die Variable um 1 erhöht
  return analogsensoren; // die Anzahl der aktiven Module wird zurückgegeben
}

/* Funktion: FarbeBerechnen()
 * Funktion, um die Farbe der LED Ampel zu berechnen
 * gibt die Farbe als String zurück.
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
