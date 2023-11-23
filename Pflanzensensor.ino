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
 * - modul_dht.h ist die Datei mit den Funktionen für den Lufttemperatur und -feuchtesensor
 * - modul_display.h ist die Datei mit den Funktionen für das Display
 * - modul_ledampel.h ist die Datei mit den Funktionen für die LED Ampel
 * - MODUL_HELLIGKEIT.h ist die Datei mit den Funktionen für den Lichtsensor
 * - modul_multiplexer.h ist die Datei mit den Funktionen für den Analogmultiplexer. Dieser
 *   kommt zum Einsatz wenn sowohl der Licht- als auch der Bodenfeuchtesensor eingesetzt
 *   werden, da beide Analogsignale liefern und der verwendete ESP8266 Chip nur einen
 *   Analogeingang hat.
 * - modul_wifi.h ist die Datei mit den Funktionen für die Wifiverbindung
 *
 * Die verwendeten Bauteile sind in der Readme.md aufgeführt.
 */

#include "Configuration.h" // Alle Einstellungen werden dort vorgenommen!

int ModuleZaehlen();
void EingebauteLedBlinken(int anzahl, int dauer);

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
  delay(1000);
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
  Serial.println(" Fabmobil Pflanzensensor, V0.2");
  module = ModuleZaehlen(); // wie viele Module sind aktiv?
  #if MODUL_DEBUG // Debuginformationen
    Serial.print(F("# Anzahl Module: "));
    Serial.println(module);
  #endif
  pinMode(pinEingebauteLed, OUTPUT); // eingebaute LED intialisieren
  EingebauteLedBlinken(1,1000); // LED leuchtet 1s zum Neustart
  #if MODUL_LEDAMPEL // wenn das LED Ampel Modul aktiv is:
    pinMode(pinAmpelGruen, OUTPUT); // LED 1 (grün)
    pinMode(pinAmpelGelb, OUTPUT); // LED 2 (gelb)
    pinMode(pinAmpelRot, OUTPUT); // LED 3 (rot)
    // alle LEDs blinken beim Start als Funktionstest:
    LedampelBlinken("gruen", 1, 1000);
    LedampelBlinken("gelb", 1, 1000);
    LedampelBlinken("rot", 1, 1000);
    #if MODUL_DEBUG // Debuginformationen
      Serial.println(F("## Setup der Ledampel"));
      Serial.print(F("# PIN gruene LED:                 ")); Serial.println(pinAmpelGruen);
      Serial.print(F("# PIN gelbe LED:                  ")); Serial.println(pinAmpelGelb);
      Serial.print(F("# PIN rote LED:                   ")); Serial.println(pinAmpelRot);
      Serial.print(F("# Bodenfeuchte Skala invertiert:  ")); Serial.println(ampelBodenfeuchteInvertiert);
      Serial.print(F("# Schwellwert Bodenfeuchte grün:  ")); Serial.println(ampelBodenfeuchteGruen);
      Serial.print(F("# Schwellwert Bodenfeuchte rot:   ")); Serial.println(ampelBodenfeuchteRot);
      Serial.print(F("# Lichtstärke Skala invertiert:   ")); Serial.println(ampelHelligkeitInvertiert);
      Serial.print(F("# Schwellwert Lichtstärke grün:   ")); Serial.println(ampelHelligkeitGruen);
      Serial.print(F("# Schwellwert Lichtstärke rot:    ")); Serial.println(ampelHelligkeitRot);
    #endif
  #endif
  #if MODUL_HELLIGKEIT || MODUL_BODENFEUCHTE // "||" ist ein logisches Oder: Wenn Helligkeits- oder Bodenfeuchtemodul aktiv ist
    pinMode(pinAnalog, INPUT);  // wird der Analogpin als Eingang gesetzt
  #endif
  #if MODUL_MULTIPLEXER // wenn das Multiplexer Modul aktiv ist werden die zwei Multiplexerpins als Ausgang gesetzt:
    pinMode(pinMultiplexer1, OUTPUT);
    pinMode(pinMultiplexer2, OUTPUT);
  #endif
  #if MODUL_WIFI // wenn das Wifi Modul aktiv ist:
    String ip = WifiSetup(wifiHostname); // Wifi-Verbindung herstellen und IP Adresse speichern
  #endif
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    // hier wird überprüft, ob die Verbindung zum Display erfolgreich war
    if(!display.begin(SSD1306_SWITCHCAPVCC, displayAdresse)) {
      Serial.println(F("Display konnte nicht geöffnet werden."));
    }
    display.display(); // Display anschalten und initialen Buffer zeigen
    delay(1000); // 1 Sekunde warten
    display.clearDisplay(); // Display löschen
    // DisplayIntro(ip, wifiHostname); // Intro auf Display abspielen
  #endif
  #if MODUL_DHT // wenn das DHT Modul aktiv ist:
    // Initialisierung des Lufttemperatur und -feuchte Sensors:
    dht.begin();
    sensor_t sensor;
    #if MODUL_DEBUG // Debuginformationen
      Serial.println(F("## DHT Sensor intialisieren und auslesen"));
      dht.temperature().getSensor(&sensor);
      Serial.println(F("# Lufttemperatursensor"));
      Serial.print  (F("# Sensortyp:       ")); Serial.println(sensor.name);
      Serial.print  (F("# Treiberversion:  ")); Serial.println(sensor.version);
      Serial.print  (F("# ID:              ")); Serial.println(sensor.sensor_id);
      Serial.print  (F("# Maximalwert:     ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
      Serial.print  (F("# Minimalwert:     ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
      Serial.print  (F("# Auflösung:       ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
      // Print humidity sensor details.
      dht.humidity().getSensor(&sensor);
      Serial.println(F("# Luftfeuchtesensor"));
      Serial.print  (F("# Sensortyp:       ")); Serial.println(sensor.name);
      Serial.print  (F("# Treiberversion:  ")); Serial.println(sensor.version);
      Serial.print  (F("# ID:              ")); Serial.println(sensor.sensor_id);
      Serial.print  (F("# Maximalwert:     ")); Serial.print(sensor.max_value); Serial.println(F("%"));
      Serial.print  (F("# Minimalwert:     ")); Serial.print(sensor.min_value); Serial.println(F("%"));
      Serial.print  (F("# Auflösung:       ")); Serial.print(sensor.resolution); Serial.println(F("%"));
    #endif
  #endif
  digitalWrite(pinEingebauteLed, HIGH); // eingebaute LED ausschalten
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
  unsigned long millisAktuell = millis();

  #if MODUL_DEBUG
    Serial.println(F("############ Begin von loop() #############"));
    Serial.print(F("# status: "));
    Serial.print(status);
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
  #endif

  MDNS.update(); // MDNS updaten

  // eingebaute LED blinken soll blinken falls sie aktiv ist:
  if ( eingebauteLedAktiv ) {
    EingebauteLedBlinken(3, 50); // in jedem neuen Loop blinkt die interne LED 3x kurz
  } else {
    digitalWrite(pinEingebauteLed, HIGH); // Ausschalten
  }

  // Helligkeit messen:
  #if MODUL_HELLIGKEIT  // wenn das Helligkeit Modul aktiv ist
    if (millisAktuell - millisVorherHelligkeit >= intervallHelligkeit) {
      #if MODUL_DEBUG
        Serial.println(F("### intervallHelligkeit erreicht."));
      #endif
      millisVorherHelligkeit = millisAktuell; // neuen Wert übernehmen
      // Ggfs. Multiplexer umstellen:
      #if MODUL_MULTIPLEXER
        MultiplexerWechseln(0, 0); // Multiplexer auf Ausgang 1 stellen
        delay(500); // 0,5s warten
      #endif
      // Helligkeit messen:
      messwertHelligkeit = HelligkeitMessen();
      // Messwert in Prozent umrechnen:
      messwertHelligkeitProzent = HelligkeitUmrechnen(messwertHelligkeit, helligkeitMinimum, helligkeitMaximum);
    }
  #endif

  // Bodenfeuchte messen;
  #if MODUL_BODENFEUCHTE // wenn das Bodenfeuchte Modul aktiv is
    if (millisAktuell - millisVorherBodenfeuchte >= intervallBodenfeuchte) {
      #if MODUL_DEBUG
        Serial.println(F("### intervallBodenfeuchte erreicht."));
      #endif
      millisVorherBodenfeuchte = millisAktuell;
      // Ggfs. Multiplexer umstellen:
      #if MODUL_MULTIPLEXER
        MultiplexerWechseln(1, 0); // Multiplexer auf Ausgang 1 stellen
        delay(500); // 0,5s warten
      #endif
      // Bodenfeuchte messen
      messwertBodenfeuchte = analogRead(pinAnalog);
      // und in Prozent umrechnen
      messwertBodenfeuchteProzent = BodenfeuchteUmrechnen(messwertBodenfeuchte, bodenfeuchteMinimum, bodenfeuchteMaximum); // Skalierung auf maximal 0 bis 100
    }
  #endif

  // Luftfeuchte und -temperatur messen:
  #if MODUL_DHT // wenn das DHT Modul aktiv ist
    if (millisAktuell - millisVorherDht >= intervallDht) {
      #if MODUL_DEBUG
        Serial.println(F("### intervallDht erreicht."));
      #endif
      millisVorherDht = millisAktuell;
      messwertLufttemperatur = DhtMessenLufttemperatur(); // Lufttemperatur messen
      messwertLuftfeuchte = DhtMessenLuftfeuchte(); // Luftfeuchte messen
    }
  #endif

  // LED Ampel umschalten:
  #if MODUL_LEDAMPEL // Wenn das LED Ampel Modul aktiv ist:
    if (millisAktuell - millisVorherLedampel >= intervallLedampel) {
      #if MODUL_DEBUG // Debuginformation
        Serial.println(F("### intervallLedAmpel erreicht."));
      #endif
      millisVorherLedampel = millisAktuell;
      LedampelUmschalten(messwertHelligkeitProzent, messwertBodenfeuchteProzent); // Ampel umschalten
    }
  #endif

  // Messwerte auf dem Display anzeigen:
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist
    if (millisAktuell - millisVorherDisplay >= intervallDisplay) {
      status += 1; // status gibt an, welche Anzeige auf dem Display aktiv ist
      if ( status == 6) { // wir haben 6 unterschiedliche Bilder
        status = 0; // danach geht es von neuem los
      }
      #if MODUL_DEBUG
        Serial.print(F("### intervallDisplay erreicht. status: ")); Serial.println(status);
      #endif
      millisVorherDisplay = millisAktuell;
      // Diese Funktion kümmert sich um die Displayanzeige:
      DisplayMesswerte(messwertBodenfeuchteProzent, messwertHelligkeitProzent, messwertLuftfeuchte, messwertLufttemperatur, status);
    }
  #endif


  // Wifi und Webserver:
  #if MODUL_WIFI // wenn das Wifi-Modul aktiv ist
    Webserver.handleClient(); // der Webserver soll in jedem loop nach Anfragen schauen!
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
    if (MODUL_BODENFEUCHTE) aktiveModule++;
    if (MODUL_DEBUG) aktiveModule++;
    if (MODUL_DISPLAY) aktiveModule++;
    if (MODUL_DHT) aktiveModule++;
    if (MODUL_HELLIGKEIT) aktiveModule++;
    if (MODUL_IFTTT) aktiveModule++;
    if (MODUL_LEDAMPEL) aktiveModule++;
    if (MODUL_WIFI) aktiveModule++;
    return aktiveModule;
}

/*
 * Funktion: EingebauteLedBlinken(int anzahl, int dauer)
 * Lässt die interne LED blinken.
 * anzahl: Anzahl der Blinkvorgänge
 * dauer: Dauer der Blinkvorgänge
 */
void EingebauteLedBlinken(int anzahl, int dauer) {
  if ( eingebauteLedAktiv ) {
    for (int i=0;i<anzahl;i++){
      delay(500);
      digitalWrite(pinEingebauteLed, LOW);
      delay(dauer);
      digitalWrite(pinEingebauteLed, HIGH);
    }
  }
}
