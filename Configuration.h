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
bool eingebauteLedAktiv = false; // wird die eingebaute LED verwendet oder nicht?
#define baudrateSeriell 9600 // Baudrate der seriellen Verbindung
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
#endif
#if MODUL_DHT // falls ein Lufttemperatur- und -feuchtesensor verbaut ist:
  #define pinDht 2 // "D4"; an welchem Pin ist der Sensor angeschlossen?
  #define dhtSensortyp DHT11  // ist ein DHT11 (blau) oder ein DHT22 (weiss) Sensor verbaut?
#endif
#if MODUL_HELLIGKEIT
  /*
   * Wenn der Helligkeitsensor aktiv ist werden hier die initialen Grenzwerte
   * für den Sensor festgelegt. Diese können später auch im Admin-Webinterface
   * verändert werden.
   */
  int helligkeitMinimum = 950; // Der Rohmesswert des Sensors wenn es ganz dunkel ist
  int helligkeitMaximum = 14; // Der Rohmesswert des Sensors, wenn es ganz hell ist
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
#endif
#if MODUL_IFTTT // wenn das IFTTT Modul aktiviert ist
  #define wifiIftttPasswort "IFTTT Schlüssel" // brauchen wir einen Schlüssel
  #define wifiIftttEreignis "Fabmobil_Pflanzensensor" // und ein Ereignisnamen
#endif
#if MODUL_MULTIPLEXER // wenn der Multiplexer aktiv ist
  #define pinMultiplexer1 16 // "D0"; Pin a des Multiplexers
  #define pinMultiplexer2 0 // "D3"; Pin b des Multiplexers
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

/**
 * Setup der einzelnen Module
 * hier muss eigentlich nichts verändert werden sondern die notewendigen globalen Variablen werden hier
 * definiert.
 */
unsigned long millisVorherHelligkeit = 0;
unsigned long millisVorherBodenfeuchte = 0;
unsigned long millisVorherDht = 0;
unsigned long millisVorherLedampel = 0;
unsigned long millisVorherDisplay = 0;
const long intervallHelligkeit = 500; // Intervall der Helligkeitsmessung in Millisekunden. Vorschlag: 5000
const long intervallBodenfeuchte = 500; // Intervall der Helligkeitsmessung in Millisekunden. Vorschlag: 5000
const long intervallDht = 5000; // Intervall der Luftfeuchte- und -temperaturmessung in Millisekunden. Vorschlag: 5000
const long intervallLedampel = 15000; // Intervall des Umschaltens der LED Ampel in Millisekunden. Vorschlag: 15000
const long intervallDisplay = 5000; // Anzeigedauer der unterschiedlichen Displayseiten in Millisekunden. Vorschlag: 5000
int module;
#if MODUL_BODENFEUCHTE
  int messwertBodenfeuchte = -1;
  int messwertBodenfeuchteProzent = -1;
  #include "modul_bodenfeuchte.h"
#else
  #define messwertBodenfeuchte -1
#endif

#if MODUL_DHT
  float messwertLufttemperatur = -1;
  float messwertLuftfeuchte = -1;
  #include "modul_dht.h"
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
  #include "modul_display.h"
#endif

#if MODUL_IFTTT
  #include "modul_ifttt.h"
#endif

#if MODUL_LEDAMPEL
  #include "modul_ledampel.h"
#endif

#if MODUL_HELLIGKEIT
  int messwertHelligkeit = -1;
  int messwertHelligkeitProzent = -1;
  #include "modul_helligkeit.h"
#else
  #define messwertHelligkeitProzent -1
#endif

#if MODUL_MULTIPLEXER
  #include "modul_multiplexer.h"
#endif

#if MODUL_WIFI
  #include "modul_wifi.h"
#endif
