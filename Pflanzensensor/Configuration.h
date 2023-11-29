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
#define MODUL_ANALOG5       true // hat dein Pflanzensensor einen fünften Analogsensor?
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
#define baudrateSeriell 9600 // Baudrate der seriellen Verbindung
int intervallAnalog = 5000; // Intervall der Messung der Analogsensoren in Millisekunden. Vorschlag: 5000
#if MODUL_BODENFEUCHTE // wenn der Bodenfeuchtesensor aktiv ist:
  /*
   * Wenn der Bodenfeuchtesensor aktiv ist werden hier die initialen Grenzwerte
   * für den Sensor festgelegt. Diese können später auch im Admin-Webinterface
   * verändert werden.
   */
  String bodenfeuchteName = "Bodenfeuchte"; // Name des Sensors
  int bodenfeuchteMinimum = 900; // Der Rohmesswert des Sensors, wenn er ganz trocken ist
  int bodenfeuchteMaximum = 380; // Der Rohmesswert des Sensors, wenn er in Wasser ist
  int bodenfeuchteGruenUnten = 40; // unter Wert des grünen Bereichs
  int bodenfeuchteGruenOben = 60; // oberer Wert des grünen Bereichs
  int bodenfeuchteGelbUnten = 20; // unterer Wert des gelben Bereichs
  int bodenfeuchteGelbOben = 80; // oberer Wert des gelben Bereichs
#endif
#if MODUL_DISPLAY
  int intervallDisplay = 4874; // Anzeigedauer der unterschiedlichen Displayseiten in Millisekunden. Vorschlag: 4874
#endif
#if MODUL_DHT // falls ein Lufttemperatur- und -feuchtesensor verbaut ist:
  #define dhtPin 0 // "D3", Pin des DHT Sensors
  #define dhtSensortyp DHT11  // ist ein DHT11 (blau) oder ein DHT22 (weiss) Sensor verbaut?
  int intervallDht = 5000; // Intervall der Luftfeuchte- und -temperaturmessung in Millisekunden. Vorschlag: 5000
  int lufttemperaturGruenUnten = 19; // unter Wert des grünen Bereichs
  int lufttemperaturGruenOben = 22; // oberer Wert des grünen Bereichs
  int lufttemperaturGelbUnten = 17; // unterer Wert des gelben Bereichs
  int lufttemperaturGelbOben = 24; // oberer Wert des gelben Bereichs
  int luftfeuchteGruenUnten = 40; // unter Wert des grünen Bereichs
  int luftfeuchteGruenOben = 60; // oberer Wert des grünen Bereichs
  int luftfeuchteGelbUnten = 20; // unterer Wert des gelben Bereichs
  int luftfeuchteGelbOben = 80; // oberer Wert des gelben Bereichs
#endif
#if MODUL_HELLIGKEIT
  /*
   * Wenn der Helligkeitsensor aktiv ist werden hier die initialen Grenzwerte
   * für den Sensor festgelegt. Diese können später auch im Admin-Webinterface
   * verändert werden.
   */
  String helligkeitName = "Helligkeit"; // Name des Sensors
  int helligkeitMinimum = 1024; // Der Rohmesswert des Sensors wenn es ganz dunkel ist
  int helligkeitMaximum = 8; // Der Rohmesswert des Sensors, wenn es ganz hell ist
  int helligkeitGruenUnten = 40; // unter Wert des grünen Bereichs
  int helligkeitGruenOben = 60; // oberer Wert des grünen Bereichs
  int helligkeitGelbUnten = 20; // unterer Wert des gelben Bereichs
  int helligkeitGelbOben = 80; // oberer Wert des gelben Bereichs
#endif
#if MODUL_LEDAMPEL // falls eine LED Ampel verbaut ist:
  int ampelModus = 1; // 0: Helligkeits- und Bodenfeuchtesensor abwechselnd, 1: Helligkeitssensor, 2: Bodenfeuchtesensor
  int intervallAmpel = 5000; // Intervall des Umschaltens der LED Ampel in Millisekunden. Vorschlag: 15273
#endif
#if MODUL_IFTTT // wenn das IFTTT Modul aktiviert ist
  #define wifiIftttPasswort "IFTTT Schlüssel" // brauchen wir einen Schlüssel
  #define wifiIftttEreignis "Fabmobil_Pflanzensensor" // und ein Ereignisnamen
#endif
#if MODUL_WIFI // wenn das Wifimodul aktiv ist
  String wifiAdminPasswort = "admin"; // Passwort für das Admininterface
  String wifiHostname = "pflanzensensor"; // Das Gerät ist später unter diesem Name + .local erreichbar
  bool wifiAp = false; // true: ESP macht seinen eigenen AP auf; false: ESP verbindet sich mit fremden WLAN
  String wifiApSsid = "Fabmobil Pflanzensensor"; // SSID des WLANs, falls vom ESP selbst aufgemacht
  bool wifiApPasswortAktiviert = false; // soll das selbst aufgemachte WLAN ein Passwort haben?
  String wifiApPasswort = "geheim"; // Das Passwort für das selbst aufgemacht WLAN
  String wifiSsid = "Tommy"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword = "freibier"; // WLAN Passwort für das fremde Wifi
#endif
String analog3Name = "Analog 3"; // Name des Sensors
#if MODUL_ANALOG3 // wenn ein dritter Analogsensor verwendet wird
  int analog3Minimum = 900; // Minimalwert des Sensors
  int analog3Maximum = 380; // Maximalwert des Sensors
  int analog3GruenUnten = 40;
  int analog3GruenOben = 60;
  int analog3GelbUnten = 20;
  int analog3GelbOben = 80;
#endif
String analog4Name = "Analog 4"; // Name des Sensors
#if MODUL_ANALOG4 // wenn ein vierter Analogsensor verwendet wird
  int analog4Minimum = 900; // Minimalwert des Sensors
  int analog4Maximum = 380; // Maximalwert des Sensors
  int analog4GruenUnten = 40;
  int analog4GruenOben = 60;
  int analog4GelbUnten = 20;
  int analog4GelbOben = 80;
#endif
String analog5Name = "Analog 5"; // Name des Sensors
#if MODUL_ANALOG5 // wenn ein fünfter Analogsensor verwendet wird
  int analog5Minimum = 900; // Minimalwert des Sensors
  int analog5Maximum = 380; // Maximalwert des Sensors
  int analog5GruenUnten = 40;
  int analog5GruenOben = 60;
  int analog5GelbUnten = 20;
  int analog5GelbOben = 80;
#endif
#if MODUL_ANALOG6 // wenn ein sechster Analogsensor verwendet wird
  String analog6Name = "Analog 6"; // Name des Sensors
  int analog6Minimum = 900; // Minimalwert des Sensors
  int analog6Maximum = 380; // Maximalwert des Sensors
  int analog6GruenUnten = 40;
  int analog6GruenOben = 60;
  int analog6GelbUnten = 20;
  int analog6GelbOben = 80;
#endif
String analog7Name = "Analog 7"; // Name des Sensors
#if MODUL_ANALOG7 // wenn ein siebter Analogsensor verwendet wird
  int analog7Minimum = 900; // Minimalwert des Sensors
  int analog7Maximum = 380; // Maximalwert des Sensors
  int analog7GruenUnten = 40;
  int analog7GruenOben = 60;
  int analog7GelbUnten = 20;
  int analog7GelbOben = 80;
#endif
String analog8Name = "Analog 8"; // Name des Sensors
#if MODUL_ANALOG8 // wenn ein achter Analogsensor verwendet wird
  int analog8Minimum = 900; // Minimalwert des Sensors
  int analog8Maximum = 380; // Maximalwert des Sensors
  int analog8GruenUnten = 40;
  int analog8GruenOben = 60;
  int analog8GelbUnten = 20;
  int analog8GelbOben = 80;
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
int analog3Messwert = -1; // Variable für den Messwert des dritten Analogsensors
int analog3MesswertProzent = -1; // Variable für den Messwert des dritten Analogsensors in Prozent
int analog4Messwert = -1; // Variable für den Messwert des vierten Analogsensors
int analog4MesswertProzent = -1; // Variable für den Messwert des vierten Analogsensors in Prozent
int analog5Messwert = -1; // Variable für den Messwert des fünften Analogsensors
int analog5MesswertProzent = -1; // Variable für den Messwert des fünften Analogsensors in Prozent
int analog6Messwert = -1; // Variable für den Messwert des sechsten Analogsensors
int analog6MesswertProzent = -1; // Variable für den Messwert des sechsten Analogsensors in Prozent
int analog7Messwert = -1; // Variable für den Messwert des siebten Analogsensors
int analog7MesswertProzent = -1; // Variable für den Messwert des siebten Analogsensors in Prozent
int analog8Messwert = -1; // Variable für den Messwert des achter Analogsensors
int analog8MesswertProzent = -1; // Variable für den Messwert des achter Analogsensors in Prozent
String helligkeitFarbe = "rot";
String bodenfeuchteFarbe = "rot";
String luftfeuchteFarbe = "rot";
String lufttemperaturFarbe = "rot";
String analog3Farbe = "rot";
String analog4Farbe = "rot";
String analog5Farbe = "rot";
String analog6Farbe = "rot";
String analog7Farbe = "rot";
String analog8Farbe = "rot";
#define pinAnalog A0 // Analogpin
#
#if MODUL_BODENFEUCHTE
  int bodenfeuchteMesswert = -1;
  int bodenfeuchteMesswertProzent = -1;
#else
  int bodenfeuchteMesswert = -1;
  int bodenfeuchteMesswertProzent = -1;
  #define bodenfeuchteName "Bodenfeuchte"
#endif

#if MODUL_DHT
  float lufttemperaturMesswert = -1;
  float luftfeuchteMesswert = -1;
  #include "dht.h" // Luftfeuchte- und -temperaturmodul einbinden
#else
  #define lufttemperaturMesswert -1
  #define luftfeuchteMesswert -1
#endif

#if MODUL_IFTTT
  #include "ifttt.h" // IFTTT Modul einbinden
#endif

#if MODUL_LEDAMPEL
  bool ampelUmschalten = true; // Schaltet zwischen Bodenfeuchte- und Helligkeitsanzeige um
  #define ampelPinRot 13 // "D7"; Pin der roten LED
  #define ampelPinGelb 12 // "D6"; Pin der roten LED
  #define ampelPinGruen 14 // "D5"; Pin der gruenen LED
  #include "ledampel.h" // LED Ampel Modul einbinden
#endif

#if MODUL_HELLIGKEIT
  int helligkeitMesswert = -1;
  int helligkeitMesswertProzent = -1;
#else
  #define helligkeitMesswert -1
  #define helligkeitMesswertProzent -1
#endif

#if MODUL_MULTIPLEXER
  #define multiplexerPinA 15 // "D8"; Pin A des Multiplexers
  #define multiplexerPinB 2 // "D4"; Pin B des Multiplexers
  #define multiplexerPinC 16 // "D0"; Pin C des Multiplexers
  #include "multiplexer.h" // Multiplexermodul einbinden
#endif


#if MODUL_DISPLAY
  int status = 0; // diese Variable schaltet durch die unterschiedlichen Anzeigen des Displays
  #define displayBreite 128 // Breite des OLED-Displays in Pixeln
  #define displayHoehe 64 // Hoehe des OLED-Displays in Pixeln
  #define displayReset -1 // Display wird mit Arduino Reset Pin zurückgesetzt, wir haben keinen Restknopf..
  #define displayAdresse 0x3C // I2C Adresse des Displays
  #include "display.h" // Displaymodul einbinden
#endif

#include "variablenspeicher.h" // Funktionen für das Speichern und Laden der Variablen
#if MODUL_WIFI
  #include "wifi.h" // Wifimodul einbinden
#endif

#include "analogsensor.h" // Funktionen für die Analogsensoren
#include "mutex.h" // Mutexmodul einbinden
mutex_t mutex;
