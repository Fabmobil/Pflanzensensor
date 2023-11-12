/*
 * FABMOBIL Pflanzensensor
 * ***********************
 * Version: 0.2 vom 11.11.2023
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
 * - modul_lichtsensor.h ist die Datei mit den Funktionen für den Lichtsensor
 * - modul_multiplexer.h ist die Datei mit den Funktionen für den Analogmultiplexer. Dieser
 *   kommt zum Einsatz wenn sowohl der Licht- als auch der Bodenfeuchtesensor eingesetzt
 *   werden, da beide Analogsignale liefern und der verwendete ESP8266 Chip nur einen 
 *   Analogeingang hat.
 * - modul_wifi.h ist die Datei mit den Funktionen für die Wifiverbindung
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
  Serial.begin(VAR_BAUDRATE); // Serielle Verbindung aufbauen
  /* Serial.println() schreibt den Text in den Klammern als Ausgabe
   * auf die serielle Schnittstelle. Die kann z.B. in der Arduino-IDE
   * über Werkzeuge -> Serieller Monitor angeschaut werden so lange
   * eine USB-Verbindung zum Chip besteht. Der Unterschied zwischen
   * Serial.println() und Serial.print() ist, dass der erste Befehl
   * 
   */
  Serial.println(" Fabmobil Pflanzensensor, V0.1");
  /* Die #if ... #endif-Anweisungen werden vom Preprozessor aufgegriffen
   * welcher den Code fürs kompilieren vorbereitet. Es wird abgefragt, 
   * ob das jeweilige Modul// Initialisierung des Sensors aktiviert ist oder nicht. Dies geschieht in
   * der Configuration.h-Datei. Ist das Modul deaktiviert, wird der Code
   * zwischen dem #if und #endif ignoriert und landet nicht auf dem Chip.
   */
  #if MODUL_DEBUG
    Serial.println(F("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
    Serial.println(F("~~ Debug: Start von setup()"));
  #endif
  pinMode(PIN_EINGEBAUTELED, OUTPUT); // eingebaute LED intialisieren
  interne_led_blinken(1,1000); // LED leuchtet 1s zum Neustart
  #if MODUL_LEDAMPEL
    pinMode(PIN_LEDAMPEL_GRUENELED, OUTPUT); // LED 1 (grün)
    pinMode(PIN_LEDAMPEL_GELBELED, OUTPUT); // LED 2 (gelb)
    pinMode(PIN_LEDAMPEL_ROTELED, OUTPUT); // LED 3 (rot)
    #if MODUL_DEBUG
      Serial.println(F("## Debug: Setup der Ledampel"));
      Serial.print(F("PIN gruene LED: ")); Serial.println(PIN_LEDAMPEL_GRUENELED);
      Serial.print(F("PIN gelbe LED: ")); Serial.println(PIN_LEDAMPEL_GELBELED);
      Serial.print(F("PIN rote LED: ")); Serial.println(PIN_LEDAMPEL_ROTELED);
      Serial.print(F("Bodenfeuchte Skala invertiert: ")); Serial.println(VAR_LEDAMPEL_BODENFEUCHTE_INVERTIERT);
      Serial.print(F("Schwellwert Bodenfeuchte grün: ")); Serial.println(VAR_LEDAMPEL_BODENFEUCHTE_GRUEN);
      Serial.print(F("Schwellwert Bodenfeuchte gelb: ")); Serial.println(VAR_LEDAMPEL_BODENFEUCHTE_GELB);
      Serial.print(F("Schwellwert Bodenfeuchte rot: ")); Serial.println(VAR_LEDAMPEL_BODENFEUCHTE_ROT);
      Serial.print(F("Lichtstärke Skala invertiert: ")); Serial.println(VAR_LEDAMPEL_LICHTSTAERKE_INVERTIERT);
      Serial.print(F("Schwellwert Lichtstärke grün: ")); Serial.println(VAR_LEDAMPEL_LICHTSTAERKE_GRUEN);
      Serial.print(F("Schwellwert Lichtstärke gelb: ")); Serial.println(VAR_LEDAMPEL_LICHTSTAERKE_GELB);
      Serial.print(F("Schwellwert Lichtstärke rot: ")); Serial.println(VAR_LEDAMPEL_LICHTSTAERKE_ROT);
    #endif
  #endif
  #if MODUL_LICHTSENSOR || MODUL_BODENFEUCHTE // "||" ist ein logisches Oder
    pinMode(PIN_ANALOG, INPUT);
  #endif
  #if MODUL_MULTIPLEXER
    pinMode(PIN_MULTIPLEXER_1, OUTPUT);
    pinMode(PIN_MULTIPLEXER_2, OUTPUT);     
    pinMode(PIN_MULTIPLEXER_3, OUTPUT); 
  #endif
  String ip = "keine WLAN Verbindung.";
  #if MODUL_WIFI
    WifiSetup(); // Wifi-Verbindung herstellen
    ip = WiFi.localIP().toString();
  #endif
  #if MODUL_DISPLAY
    // hier wird überprüft, ob die Verbindung zum Display erfolgreich war
    if(!display.begin(SSD1306_SWITCHCAPVCC, VAR_DISPLAY_ADRESSE)) {
      Serial.println(F("Display konnte nicht geöffnet werden."));
    }
    display.display(); // Display anschalten und initialen Buffer zeigen
    delay(1000); // 1 Sekunde warten
    display.clearDisplay(); // Display löschen
    DisplayIntro(ip); // Introfunktion aufrufen
  #endif
  #if MODUL_DHT
    // Initialisierung des Lufttemperatur und -feuchte Sensors:
    dht.begin(); 
    sensor_t sensor;
    #if MODUL_DEBUG
      Serial.println(F("## Debug: DHT Sensor intialisieren und auslesen"));
      Serial.println(F("#######################################"));
      dht.temperature().getSensor(&sensor);
      Serial.println(F("Temperatursensor"));
      Serial.print  (F("Sensortyp: ")); Serial.println(sensor.name);
      Serial.print  (F("Treiberversion:  ")); Serial.println(sensor.version);
      Serial.print  (F("ID:   ")); Serial.println(sensor.sensor_id);
      Serial.print  (F("Maximalwert:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
      Serial.print  (F("Minimalwert:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
      Serial.print  (F("Auflösung:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
      Serial.println(F("------------------------------------"));
      // Print humidity sensor details.
      dht.humidity().getSensor(&sensor);
      Serial.println(F("Humidity Sensor"));
      Serial.print  (F("Sensortyp: ")); Serial.println(sensor.name);
      Serial.print  (F("Treiberversion:  ")); Serial.println(sensor.version);
      Serial.print  (F("ID:   ")); Serial.println(sensor.sensor_id);
      Serial.print  (F("Maximalwert:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
      Serial.print  (F("Minimalwert:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
      Serial.print  (F("Auflösung:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
    #endif
  #endif
  #if MODUL_DEBUG
    Serial.println(F("~~ Debug: Ende von setup()"));
    Serial.println(F("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
  #endif
}

/*
 * Funktion: loop()
 * Zentrale Schleifenfunktion die immer wieder neu ausgeführt wird wenn sie abgeschlossen ist.
 */
void loop() {
  #if MODUL_DEBUG
    Serial.println(F("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
    Serial.println(F("~~ Debug: Begin von loop()"));
  #endif
  interne_led_blinken(3, 50); // in jedem neuen Loop blinkt die interne LED 3x kurz
  
  // Lichtsensor messen und ggfs. Multiplexer umstellen
  #if MODUL_LICHTSENSOR
    #if MODUL_MULTIPLEXER
      MultiplexerWechseln(LOW, LOW, HIGH); // Multiplexer auf Ausgang 2 stellen
      delay(500); // 0,5s warten
    #endif
  LICHTSTAERKE_MESSWERT = LichtsensorMessen();
  int LICHTSTAERKE_PROZENT = LichtsensorUmrechnen(LICHTSTAERKE_MESSWERT, VAR_LICHTSTAERKE_MIN, VAR_LICHTSTAERKE_MAX);
  #endif

  // Bodenfeuchte messen und ggfs. Multiplexer umstellen
  #if MODUL_BODENFEUCHTE
    #if MODUL_MULTIPLEXER
      MultiplexerWechseln(LOW, LOW, LOW); // Multiplexer auf Ausgang 1 stellen
      delay(500); // 0,5s warten
    #endif
    // Bodenfeuchte messen
    Serial.print("Messwert Bodenfeuchte: ");
    int MESSWERT_BODENFEUCHTE = analogRead(PIN_ANALOG);
    int PROZENT_BODENFEUCHTE = BodenfeuchteUmrechnung(MESSWERT_BODENFEUCHTE); // Skalierung auf maximal 0 bis 100
    Serial.println(MESSWERT_BODENFEUCHTE);
  #endif

  // Luftfeuchte und -temperatur messen
  #if MODUL_DHT
    //float MESSWERT_LUFTTEMPERATUR = 23.3;
    //float MESSWERT_LUFTFEUCHTE = 39.9;
    float MESSWERT_LUFTTEMPERATUR = DhtMessenLufttemperatur();
    float MESSWERT_LUFTFEUCHTE = DhtMessenLuftfeuchte();
  #endif

  // LED Ampel anzeigen lassen
  #if MODUL_LEDAMPEL
    // Modus 1: Anzeige Lichtstärke
    #if MODUL_LICHTSENSOR
      /*
       * Falls es auch das Bodenfeuchte Modul gibt, blinkt die LED Ampel kurz 5x gelb damit
       * klar ist, dass jetzt die Lichtstärke angezeigt wird..
       */
      #if MODUL_BODENFEUCHTE
        // LED Ampel blinkt 5x gelb um zu signalisieren, dass jetzt der Bodenfeuchtewert angezeigt wird
        LedampelBlinken("gekb", 5, 100);
      #endif
      // Unterscheidung, ob die Skala der Lichtstärke invertiert ist oder nicht
      if ( VAR_LEDAMPEL_LICHTSTAERKE_INVERTIERT ) {
        if ( LICHTSTAERKE_MESSWERT >= VAR_LEDAMPEL_LICHTSTAERKE_GRUEN ) {
          LedampelAnzeigen("gruen", -1);   
        }
        if ( (LICHTSTAERKE_MESSWERT >= VAR_LEDAMPEL_LICHTSTAERKE_GELB) && (LICHTSTAERKE_MESSWERT < VAR_LEDAMPEL_LICHTSTAERKE_GRUEN) ) {
          LedampelAnzeigen("gelb", -1);   
        }
        if ( LICHTSTAERKE_MESSWERT < VAR_LEDAMPEL_LICHTSTAERKE_GELB ) {
          LedampelAnzeigen("rot", -1);   
        } 
      } else {
        if ( LICHTSTAERKE_MESSWERT <= VAR_LEDAMPEL_LICHTSTAERKE_GRUEN ) {
          LedampelAnzeigen("gruen", -1);   
        }
        if ( (LICHTSTAERKE_MESSWERT <= VAR_LEDAMPEL_LICHTSTAERKE_GELB) && (LICHTSTAERKE_MESSWERT < VAR_LEDAMPEL_LICHTSTAERKE_GRUEN) ) {
          LedampelAnzeigen("gelb", -1);   
        }
        if ( LICHTSTAERKE_MESSWERT > VAR_LEDAMPEL_LICHTSTAERKE_GELB ) {
          LedampelAnzeigen("rot", -1);   
        }
      }
    #endif
    // Modus 2: Anzeige der Bodenfeuchte
    #if MODUL_BODENFEUCHTE
      #if MODUL_LICHTSTAERKE
      /*
       * Falls es auch das Bodenfeuchte Modul gibt, blinkt die LED Ampel kurz 5x grün damit
       * klar ist, dass jetzt die Bodenfeuchte angezeigt wird.
       */
        LedampelBlinken("gruen", 5, 100);
      #endif
      if ( VAR_LEDAMPEL_BODENFEUCHTE_INVERTIERT ) { // Unterscheidung, ob die Bodenfeuchteskala invertiert wird oder nicht
        if ( MESSWERT_BODENFEUCHTE >= VAR_LEDAMPEL_BODENFEUCHTE_GRUEN ) {
          LedampelAnzeigen("gruen", -1);   
        }
        if ( (MESSWERT_BODENFEUCHTE >= VAR_LEDAMPEL_BODENFEUCHTE_GELB) && (MESSWERT_BODENFEUCHTE < VAR_LEDAMPEL_BODENFEUCHTE_GRUEN) ) {
          LedampelAnzeigen("gelb", -1);   
        }
        if ( MESSWERT_BODENFEUCHTE < VAR_LEDAMPEL_BODENFEUCHTE_GELB ) {
          LedampelAnzeigen("rot", -1);   
        }
      } else {
        if ( MESSWERT_BODENFEUCHTE <= VAR_LEDAMPEL_BODENFEUCHTE_GRUEN ) {
          LedampelAnzeigen("gruen", -1);   
        }
        if ( (MESSWERT_BODENFEUCHTE <= VAR_LEDAMPEL_BODENFEUCHTE_GELB) && (MESSWERT_BODENFEUCHTE < VAR_LEDAMPEL_BODENFEUCHTE_GRUEN) ) {
          LedampelAnzeigen("gelb", -1);   
        }
        if ( MESSWERT_BODENFEUCHTE > VAR_LEDAMPEL_BODENFEUCHTE_GELB ) {
          LedampelAnzeigen("rot", -1);   
        }
      }
    #endif
  #endif
  
  // Messwerte auf dem Display anzeigen
  #if MODUL_DISPLAY
    DisplayMesswerte(MESSWERT_BODENFEUCHTE, LICHTSTAERKE_PROZENT, MESSWERT_LUFTFEUCHTE, MESSWERT_LUFTTEMPERATUR);
  #endif
  #if MODUL_DEBUG
    Serial.println(F("~~ Debug: Ende von loop()"));
    Serial.println(F("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"));
    Serial.println(F(" "));
  #endif

  // Wifi und Webserver
  #if MODUL_WIFI
    server.handleClient(); 
  #endif
}

/*
 * Funktion: interne_led_blinken(int anzahl, int dauer)
 * Lässt die interne LED blinken.
 * anzahl: Anzahl der Blinkvorgänge
 * dauer: Dauer der Blinkvorgänge
 */
void interne_led_blinken(int anzahl, int dauer){
  for (int i=0;i<anzahl;i++){
    delay(500);
    digitalWrite(PIN_EINGEBAUTELED, LOW);
    delay(dauer);
    digitalWrite(PIN_EINGEBAUTELED, HIGH);
  } 
}
