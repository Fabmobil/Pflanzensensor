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
  #if MODUL_DEBUG
    Serial.println(F("#### Debug: Start von setup()"));
  #endif
  /* Serial.println() schreibt den Text in den Klammern als Ausgabe
   * auf die serielle Schnittstelle. Die kann z.B. in der Arduino-IDE
   * über Werkzeuge -> Serieller Monitor angeschaut werden so lange
   * eine USB-Verbindung zum Chip besteht. Der Unterschied zwischen
   * Serial.println() und Serial.print() ist, dass der erste Befehl
   *
   */
  Serial.println(" Fabmobil Pflanzensensor, V0.2");
  pinMode(pinEingebauteLed, OUTPUT); // eingebaute LED intialisieren
  EingebauteLedBlinken(1,1000); // LED leuchtet 1s zum Neustart
  #if MODUL_LEDAMPEL
    pinMode(pinAmpelGruen, OUTPUT); // LED 1 (grün)
    pinMode(pinAmpelGelb, OUTPUT); // LED 2 (gelb)
    pinMode(pinAmpelRot, OUTPUT); // LED 3 (rot)
    // LedampelBlinken("gruen", 1, 2000);
    // LedampelBlinken("gelb", 1, 2000);
    // LedampelBlinken("rot", 1, 2000);
    #if MODUL_DEBUG
      Serial.println(F("## Debug: Setup der Ledampel"));
      Serial.print(F("PIN gruene LED: ")); Serial.println(pinAmpelGruen);
      Serial.print(F("PIN gelbe LED: ")); Serial.println(pinAmpelGelb);
      Serial.print(F("PIN rote LED: ")); Serial.println(pinAmpelRot);
      Serial.print(F("Bodenfeuchte Skala invertiert: ")); Serial.println(ampelBodenfeuchteInvertiert);
      Serial.print(F("Schwellwert Bodenfeuchte grün: ")); Serial.println(ampelBodenfeuchteGruen);
      Serial.print(F("Schwellwert Bodenfeuchte gelb: ")); Serial.println(ampelBodenfeuchteGelb);
      Serial.print(F("Schwellwert Bodenfeuchte rot: ")); Serial.println(ampelBodenfeuchteRot);
      Serial.print(F("Lichtstärke Skala invertiert: ")); Serial.println(ampelHelligkeitInvertiert);
      Serial.print(F("Schwellwert Lichtstärke grün: ")); Serial.println(ampelHelligkeitGruen);
      Serial.print(F("Schwellwert Lichtstärke gelb: ")); Serial.println(ampelHelligkeitGelb);
      Serial.print(F("Schwellwert Lichtstärke rot: ")); Serial.println(ampelHelligkeitRot);
    #endif
  #endif
  #if MODUL_HELLIGKEIT || MODUL_BODENFEUCHTE // "||" ist ein logisches Oder
    pinMode(pinAnalog, INPUT);
  #endif
  #if MODUL_MULTIPLEXER
    pinMode(pinMultiplexer, OUTPUT);
  #endif
  String ip = "keine WLAN Verbindung.";
  #if MODUL_WIFI
    WifiSetup(wifiHostname); // Wifi-Verbindung herstellen
    ip = WiFi.localIP().toString();
  #endif
  #if MODUL_DISPLAY
    // hier wird überprüft, ob die Verbindung zum Display erfolgreich war
    if(!display.begin(SSD1306_SWITCHCAPVCC, displayAdresse)) {
      Serial.println(F("Display konnte nicht geöffnet werden."));
    }
    display.display(); // Display anschalten und initialen Buffer zeigen
    delay(1000); // 1 Sekunde warten
    display.clearDisplay(); // Display löschen
    // DisplayIntro(ip, wifiHostname); // Introfunktion aufrufen
  #endif
  #if MODUL_DHT
    // Initialisierung des Lufttemperatur und -feuchte Sensors:
    dht.begin();
    sensor_t sensor;
    #if MODUL_DEBUG
      Serial.println(F("## Debug: DHT Sensor intialisieren und auslesen"));
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
}

/*
 * Funktion: loop()
 * Zentrale Schleifenfunktion die immer wieder neu ausgeführt wird wenn sie abgeschlossen ist.
 */
void loop() {
  Serial.println(F("####### Begin von loop() #######"));
  if ( eingebauteLedAktiv ) {
    EingebauteLedBlinken(3, 50); // in jedem neuen Loop blinkt die interne LED 3x kurz
  } else {

  };
  // Lichtsensor messen und ggfs. Multiplexer umstellen
  #if MODUL_HELLIGKEIT
    #if MODUL_MULTIPLEXER
      MultiplexerWechseln(0); // Multiplexer auf Ausgang 1 stellen
      delay(500); // 0,5s warten
    #endif
  messwertHelligkeit = HelligkeitMessen();
  int messwertHelligkeitProzent = HelligkeitUmrechnen(messwertHelligkeit, helligkeitMinimum, helligkeitMaximum);
  #endif

  // Bodenfeuchte messen und ggfs. Multiplexer umstellen
  #if MODUL_BODENFEUCHTE
    #if MODUL_MULTIPLEXER
      MultiplexerWechseln(1); // Multiplexer auf Ausgang 1 stellen
      delay(500); // 0,5s warten
    #endif
    // Bodenfeuchte messen
    messwertBodenfeuchte = analogRead(pinAnalog);
    messwertBodenfeuchteProzent = BodenfeuchteUmrechnung(messwertBodenfeuchte); // Skalierung auf maximal 0 bis 100
  #endif

  // Luftfeuchte und -temperatur messen
  #if MODUL_DHT
    //float messwertLufttemperatur = 23.3;
    //float messwertLuftfeuchte = 39.9;
    messwertLufttemperatur = DhtMessenLufttemperatur();
    messwertLuftfeuchte = DhtMessenLuftfeuchte();
  #endif

  // LED Ampel anzeigen lassen
  #if MODUL_LEDAMPEL
    switch (status) {  // nur aller 4 Zyklen Anzeige wechseln
    case 0:
      // Modus 1: Anzeige Lichtstärke
      #if MODUL_HELLIGKEIT
      /*
      * Falls es auch das Bodenfeuchte Modul gibt, blinkt die LED Ampel kurz 5x gelb damit
      * klar ist, dass jetzt die Lichtstärke angezeigt wird..
      */
      if ( !MODUL_BODENFEUCHTE  ) {
        // LED Ampel blinkt 5x gelb um zu signalisieren, dass jetzt der Bodenfeuchtewert angezeigt wird
        ampelUmschalten = true;
      } else {
        ampelUmschalten = !ampelUmschalten; // wir wollen nur jede zweite Runde umschalten
      }
      // Unterscheidung, ob die Skala der Lichtstärke invertiert ist oder nicht
      if ( ampelUmschalten ) {
        if ( MODUL_BODENFEUCHTE ) { LedampelBlinken("gelb", 2, 500); }
        #if MODUL_DEBUG
          Serial.print(F("ampelUmschalten = "));
          Serial.print(ampelUmschalten);
          Serial.println(F(": Ledampel zeigt Helligkeit an."));
        #endif
        if ( ampelHelligkeitInvertiert ) {
          if ( messwertHelligkeit >= ampelHelligkeitGruen ) {
            LedampelAnzeigen("gruen", -1);
          }
          if ( (messwertHelligkeit >= ampelHelligkeitGelb) && (messwertHelligkeit < ampelHelligkeitGruen) ) {
            LedampelAnzeigen("gelb", -1);
          }
          if ( messwertHelligkeit < ampelHelligkeitGelb ) {
            LedampelAnzeigen("rot", -1);
          }
        } else {
          if ( messwertHelligkeit <= ampelHelligkeitGruen ) {
            LedampelAnzeigen("gruen", -1);
          }
          if ( (messwertHelligkeit <= ampelHelligkeitGelb) && (messwertHelligkeit < ampelHelligkeitGruen) ) {
            LedampelAnzeigen("gelb", -1);
          }
          if ( messwertHelligkeit > ampelHelligkeitGelb ) {
            LedampelAnzeigen("rot", -1);
          }
        }
      }
      #endif
      // Modus 2: Anzeige der Bodenfeuchte
      #if MODUL_BODENFEUCHTE
        if ( !MODUL_HELLIGKEIT ) { // Falls es kein Helligkeitsmodul gibt
          ampelUmschalten = false;
        }
        if ( !ampelUmschalten ) {
          #if MODUL_DEBUG
            Serial.print(F("ampelUmschalten = "));
            Serial.print(ampelUmschalten);
            Serial.println(F(": Ledampel zeigt Bodenfeuchte an."));
          #endif
          if ( MODUL_HELLIGKEIT ) { LedampelBlinken("gruen", 2, 500); }
          if ( ampelBodenfeuchteInvertiert ) { // Unterscheidung, ob die Bodenfeuchteskala invertiert wird oder nicht
            if ( messwertBodenfeuchte >= ampelBodenfeuchteGruen ) {
              LedampelAnzeigen("gruen", -1);
            }
            if ( (messwertBodenfeuchte >= ampelBodenfeuchteGelb) && (messwertBodenfeuchte < ampelBodenfeuchteGruen) ) {
              LedampelAnzeigen("gelb", -1);
            }
            if ( messwertBodenfeuchte < ampelBodenfeuchteGelb ) {
              LedampelAnzeigen("rot", -1);
            }
          } else {
            if ( messwertBodenfeuchte <= ampelBodenfeuchteGruen ) {
              LedampelAnzeigen("gruen", -1);
            }
            if ( (messwertBodenfeuchte <= ampelBodenfeuchteGelb) && (messwertBodenfeuchte < ampelBodenfeuchteGruen) ) {
              LedampelAnzeigen("gelb", -1);
            }
            if ( messwertBodenfeuchte > ampelBodenfeuchteGelb ) {
              LedampelAnzeigen("rot", -1);
            }
          }
        }
      #endif
    #endif
  }
  // Messwerte auf dem Display anzeigen
  #if MODUL_DISPLAY
    DisplayMesswerte(messwertBodenfeuchte, messwertHelligkeitProzent, messwertLuftfeuchte, messwertLufttemperatur, status);
  #endif
  #if MODUL_DEBUG
    Serial.println(F(" "));
  #endif

  // Wifi und Webserver
  #if MODUL_WIFI
    Webserver.handleClient();
  #endif
  status = (status + 1) % 4; // Statuszähler erhöhen. Mögliche Zustände: 0, 1, 2, 3
}

/*
 * Funktion: EingebauteLedBlinken(int anzahl, int dauer)
 * Lässt die interne LED blinken.
 * anzahl: Anzahl der Blinkvorgänge
 * dauer: Dauer der Blinkvorgänge
 */
void EingebauteLedBlinken(int anzahl, int dauer){
  if ( eingebauteLedAktiv ) {
    for (int i=0;i<anzahl;i++){
      delay(500);
      digitalWrite(pinEingebauteLed, LOW);
      delay(dauer);
      digitalWrite(pinEingebauteLed, HIGH);
    }
  }
}
