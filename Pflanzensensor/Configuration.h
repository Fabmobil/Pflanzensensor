/**
 * Fabmobil Pflanzensensor Konfigurationsdatei
 *
 * hier werden die Module ausgewählt sowie die PINs und
 * Variablen festgelegt.
 */

/**
 * Module
 * "true" aktiviert sie, "false" deaktiviert sie
 */
#define MODUL_DEBUG         false  // Debugmodus (de)aktivieren
#define MODUL_DISPLAY       true  // hat dein Pflanzensensor ein Display?
#define MODUL_WIFI          true // verwendet dein Pflanzensensor das WiFi-Modul?
#define MODUL_DHT           true // hat dein Pflanzensensor ein Luftfeuchte- und Temperaturmesser?
#define MODUL_BODENFEUCHTE  true // hat dein Pflanzensensor einen Bodenfeuchtemesser?
#define MODUL_LEDAMPEL      true // hat dein Pflanzensensor eine LED Ampel?
#define MODUL_HELLIGKEIT    true // hat dein Pflanzensensor einen Lichtsensor?
#define MODUL_IFTTT         false // willst du das ifttt.com-Modul verwenden?
#define MODUL_ANALOG3       true // hat dein Pflanzensensor einen dritten Analogsensor?
#define MODUL_ANALOG4       true // hat dein Pflanzensensor einen vierten Analogsensor?
#define MODUL_ANALOG5       false // hat dein Pflanzensensor einen fünften Analogsensor?
#define MODUL_ANALOG6       false // hat dein Pflanzensensor einen sechsten Analogsensor?
#define MODUL_ANALOG7       false // hat dein Pflanzensensor einen siebten Analogsensor?
#define MODUL_ANALOG8       false // hat dein Pflanzensensor einen achten Analogsensor?
// Wenn Bodenfeuchte- und Lichtsensor verwendet werden, brauchen wir auch einen Analog-Multiplexer:
#if MODUL_BODENFEUCHTE && MODUL_HELLIGKEIT
  #define MODUL_MULTIPLEXER true
#else
  #define MODUL_MULTIPLEXER false //sonst nicht
#endif

/**
 * Pinbelegungen und Variablen
 */
#define pinEingebauteLed LED_BUILTIN // "D0"; worüber wird die interne LED des ESPs angesprochen?
#define baudrateSeriell 9600 // Baudrate der seriellen Verbindung
 const long intervallAnalog = 5000; // Intervall der Messung des dritten Analogsensors in Millisekunden. Vorschlag: 5000
#if MODUL_BODENFEUCHTE || MODUL_HELLIGKEIT // Wenn wir Analogsensoren benutzen
  #define pinAnalog A0 // definieren wir hier den Analogpin
#endif
#if MODUL_BODENFEUCHTE // wenn der Bodenfeuchtesensor aktiv ist:
  /*
   * Wenn der Bodenfeuchtesensor aktiv ist werden hier die initialen Grenzwerte
   * für den Sensor festgelegt. Diese können später auch im Admin-Webinterface
   * verändert werden.
   */
  int bodenfeuchteMinimum = 900; // Der Rohmesswert des Sensors, wenn er ganz trocken ist
  int bodenfeuchteMaximum = 380; // Der Rohmesswert des Sensors, wenn er in Wasser ist
  String bodenfeuchteName = "Bodenfeuchte"; // Name des Sensors
#endif
#if MODUL_DISPLAY
  const long intervallDisplay = 4874; // Anzeigedauer der unterschiedlichen Displayseiten in Millisekunden. Vorschlag: 4874
#endif
#if MODUL_DHT // falls ein Lufttemperatur- und -feuchtesensor verbaut ist:
  #define pinDht 0 // "D3", Pin des DHT Sensors
  #define dhtSensortyp DHT11  // ist ein DHT11 (blau) oder ein DHT22 (weiss) Sensor verbaut?
  const long intervallDht = 5000; // Intervall der Luftfeuchte- und -temperaturmessung in Millisekunden. Vorschlag: 5000
#endif
#if MODUL_HELLIGKEIT
  /*
   * Wenn der Helligkeitsensor aktiv ist werden hier die initialen Grenzwerte
   * für den Sensor festgelegt. Diese können später auch im Admin-Webinterface
   * verändert werden.
   */
  int helligkeitMinimum = 950; // Der Rohmesswert des Sensors wenn es ganz dunkel ist
  int helligkeitMaximum = 14; // Der Rohmesswert des Sensors, wenn es ganz hell ist
  String helligkeitName = "Helligkeit"; // Name des Sensors
#endif
#if MODUL_LEDAMPEL // falls eine LED Ampel verbaut ist:
  #define pinAmpelRot 13 // "D7"; Pin der roten LED
  #define pinAmpelGelb 12 // "D6"; Pin der roten LED
  #define pinAmpelGruen 14 // "D5"; Pin der gruenen LED
  bool ampelBodenfeuchteInvertiert = false; // true: grün = großer Wert, rot = kleiner Wert. false: rot = großer Wert, grün = kleiner Wert
  int ampelBodenfeuchteGruen = 10; // Luftfeuchte in %, ab der die Ampel grün ist
  int ampelBodenfeuchteRot = 90; // Luftfeuchte in %, ab der die Ampel grün ist
  bool ampelHelligkeitInvertiert = true; // true: grün = kleiner Wert, rot = großer Wert. false: rot = kleiner Wert, grün = großer Wert
  int ampelHelligkeitGruen = 90; // Lichtstärke in %, bis zu der Ampel grün ist
  int ampelHelligkeitRot = 10; // Lichtstärke in %, bis zu der Ampel grün ist
  int ampelModus = 0; // 0: Helligkeits- und Bodenfeuchtesensor abwechselnd, 1: Helligkeitssensor, 2: Bodenfeuchtesensor
  bool ampelUmschalten = true; // Schaltet zwischen Bodenfeuchte- und Helligkeitsanzeige um
  const long intervallLedampel = 15273; // Intervall des Umschaltens der LED Ampel in Millisekunden. Vorschlag: 15273
#endif
#if MODUL_IFTTT // wenn das IFTTT Modul aktiviert ist
  #define wifiIftttPasswort "IFTTT Schlüssel" // brauchen wir einen Schlüssel
  #define wifiIftttEreignis "Fabmobil_Pflanzensensor" // und ein Ereignisnamen
#endif
#if MODUL_MULTIPLEXER // wenn der Multiplexer aktiv ist
  #define pinMultiplexerA 15 // "D8"; Pin A des Multiplexers
  #define pinMultiplexerB 2 // "D4"; Pin B des Multiplexers
  #define pinMultiplexerC 16 // "D0"; Pin C des Multiplexers
#endif
#if MODUL_WIFI // wenn das Wifimodul aktiv ist
  String wifiAdminPasswort = "admin"; // Passwort für das Admininterface
  String wifiHostname = "pflanzensensor"; // Das Gerät ist später unter diesem Name + .local erreichbar
  #define wifiAp false // true: ESP macht seinen eigenen AP auf; false: ESP verbindet sich mit fremden WLAN
  #define wifiApSsid "Fabmobil Pflanzensensor" // SSID des WLANs, falls vom ESP selbst aufgemacht
  #define wifiApPasswortAktiviert false // soll das selbst aufgemachte WLAN ein Passwort haben?
  #define wifiApPasswort "geheim" // Das Passwort für das selbst aufgemacht WLAN
  #define wifiSsid "Tommy" // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  #define wifiPassword "freibier" // WLAN Passwort für das fremde Wifi
#endif
String analog3Name = "Analog 3"; // Name des Sensors
#if MODUL_ANALOG3 // wenn ein dritter Analogsensor verwendet wird
  int analog3Minimum = 900; // Minimalwert des Sensors
  int analog3Maximum = 380; // Maximalwert des Sensors
#endif
String analog4Name = "Analog 4"; // Name des Sensors
#if MODUL_ANALOG4 // wenn ein vierter Analogsensor verwendet wird
  int analog4Minimum = 900; // Minimalwert des Sensors
  int analog4Maximum = 380; // Maximalwert des Sensors
#endif
String analog5Name = "Analog 5"; // Name des Sensors
#if MODUL_ANALOG5 // wenn ein fünfter Analogsensor verwendet wird
  int analog5Minimum = 900; // Minimalwert des Sensors
  int analog5Maximum = 380; // Maximalwert des Sensors
#endif
String analog6Name = "Analog 6"; // Name des Sensors
#if MODUL_ANALOG6 // wenn ein sechster Analogsensor verwendet wird
  int analog6Minimum = 900; // Minimalwert des Sensors
  int analog6Maximum = 380; // Maximalwert des Sensors
#endif
String analog7Name = "Analog 7"; // Name des Sensors
#if MODUL_ANALOG7 // wenn ein siebter Analogsensor verwendet wird
  int analog7Minimum = 900; // Minimalwert des Sensors
  int analog7Maximum = 380; // Maximalwert des Sensors
#endif
String analog8Name = "Analog 8"; // Name des Sensors
#if MODUL_ANALOG8 // wenn ein achter Analogsensor verwendet wird
  int analog8Minimum = 900; // Minimalwert des Sensors
  int analog8Maximum = 380; // Maximalwert des Sensors
#endif

/**
 * Setup der einzelnen Module
 * hier muss eigentlich nichts verändert werden sondern die notewendigen globalen Variablen werden hier
 * definiert.
 */
unsigned long millisVorherAnalog = 0; // Variable für die Messung des Intervalls der Analogsensormessung
unsigned long millisVorherDht = 0; // Variable für die Messung des Intervalls der Luftfeuchte- und -temperaturmessung
unsigned long millisVorherLedampel = 0; // Variable für die Messung des Intervalls des Umschaltens der LED Ampel
unsigned long millisVorherDisplay = 0; // Variable für die Messung des Intervalls der Anzeige des Displays
int module; // Variable für die Anzahl der Module
int displayseiten; // Variable für die Anzahl der Analogsensoren
int messwertAnalog3 = -1; // Variable für den Messwert des dritten Analogsensors
int messwertAnalog3Prozent = -1; // Variable für den Messwert des dritten Analogsensors in Prozent
int messwertAnalog4 = -1; // Variable für den Messwert des vierten Analogsensors
int messwertAnalog4Prozent = -1; // Variable für den Messwert des vierten Analogsensors in Prozent
int messwertAnalog5 = -1; // Variable für den Messwert des fünften Analogsensors
int messwertAnalog5Prozent = -1; // Variable für den Messwert des fünften Analogsensors in Prozent
int messwertAnalog6 = -1; // Variable für den Messwert des sechsten Analogsensors
int messwertAnalog6Prozent = -1; // Variable für den Messwert des sechsten Analogsensors in Prozent
int messwertAnalog7 = -1; // Variable für den Messwert des siebten Analogsensors
int messwertAnalog7Prozent = -1; // Variable für den Messwert des siebten Analogsensors in Prozent
int messwertAnalog8 = -1; // Variable für den Messwert des achter Analogsensors
int messwertAnalog8Prozent = -1; // Variable für den Messwert des achter Analogsensors in Prozent

#if MODUL_BODENFEUCHTE
  int messwertBodenfeuchte = -1;
  int messwertBodenfeuchteProzent = -1;
#else
  int messwertBodenfeuchte = -1;
  int messwertBodenfeuchteProzent = -1;
#endif

#if MODUL_DHT
  float messwertLufttemperatur = -1;
  float messwertLuftfeuchte = -1;
  #include "modul_dht.h" // Luftfeuchte- und -temperaturmodul einbinden
#else
  #define messwertLufttemperatur -1
  #define messwertLuftfeuchte -1
#endif

#if MODUL_DISPLAY
  int status = 0; // diese Variable schaltet durch die unterschiedlichen Anzeigen des Displays
  #define displayBreite 128 // Breite des OLED-Displays in Pixeln
  #define displayHoehe 64 // Hoehe des OLED-Displays in Pixeln
  #define displayReset -1 // Display wird mit Arduino Reset Pin zurückgesetzt, wir haben keinen Restknopf..
  #define displayAdresse 0x3C // I2C Adresse des Displays
  #include "modul_display.h" // Displaymodul einbinden
#endif

#if MODUL_IFTTT
  #include "modul_ifttt.h" // IFTTT Modul einbinden
#endif

#if MODUL_LEDAMPEL
  #include "modul_ledampel.h" // LED Ampel Modul einbinden
#endif

#if MODUL_HELLIGKEIT
  int messwertHelligkeit = -1;
  int messwertHelligkeitProzent = -1;
#else
  #define messwertHelligkeit -1
  #define messwertHelligkeitProzent -1
#endif

#if MODUL_MULTIPLEXER
  #include "modul_multiplexer.h" // Multiplexermodul einbinden
#endif

#if MODUL_WIFI
  #include "modul_wifi.h" // Wifimodul einbinden
#endif
