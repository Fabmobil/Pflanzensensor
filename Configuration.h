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
// Wenn Bodenfeuchte- und Lichtsensor verwendet werden, brauchen wir auch einen Analog-Multiplexer:
#if MODUL_BODENFEUCHTE && MODUL_HELLIGKEIT
  #define MODUL_MULTIPLEXER true
#else
  #define MODUL_MULTIPLEXER false
#endif

/**
 * Pinbelegungen und Variablen
 */
#define pinEingebauteLed LED_BUILTIN // "D0"; worüber wird die interne LED des ESPs angesprochen?
bool eingebauteLedAktiv = false; // wird die eingebaute LED verwendet oder nicht?
#define baudrateSeriell 9600 // Baudrate der seriellen Verbindung
#if MODUL_BODENFEUCHTE || MODUL_HELLIGKEIT
  #define pinAnalog A0
#endif
#if MODUL_DHT
  #define pinDht 2 // "D4"; an welchem Pin ist der Sensor angeschlossen?
  #define dhtSensortyp DHT22  // ist ein DHT11 (blau) oder ein DHT22 (weiss) Sensor verbaut?
#endif
#if MODUL_DISPLAY
  int statusdauer = 1500; // Anzeigedauer der einzelnen Messwerte auf dem Display in Millisekunden
  int status = 0;
#endif
#if MODUL_LEDAMPEL
  #define pinAmpelRot 12 // "D6"; Pin der roten LED
  #define pinAmpelGelb 13 // "D7"; Pin der gelben LED
  #define pinAmpelGruen 14 // "D5"; Pin der gruenen LED
  bool ampelBodenfeuchteInvertiert = false; // true: grün = großer Wert, rot = kleiner Wert. false: rot = großer Wert, grün = kleiner Wert
  int ampelBodenfeuchteGruen = 40; // Luftfeuchte in %, ab der die Ampel grün ist
  int ampelBodenfeuchteGelb = 30; // Luftfeuchte in %, ab der die Ampel gelb und unter der die Ampel rot ist
  int ampelBodenfeuchteRot = 40; // Luftfeuchte in %, ab der die Ampel grün ist
  bool ampelHelligkeitInvertiert = false; // true: grün = klein, rot = groß. false: rot = klein, grün = groß
  int ampelHelligkeitGruen = 50; // Lichtstärke in %, bis zu der Ampel grün ist
  int ampelHelligkeitGelb = 30; // Lichtstärke in %, bis zu der Ampel grün ist
  int ampelHelligkeitRot = 10; // Lichtstärke in %, bis zu der Ampel grün ist
  bool ampelUmschalten = true;
#endif
#if MODUL_HELLIGKEIT
  int helligkeitMinimum = 900;
  int helligkeitMaximum = 20;
#endif
#if MODUL_MULTIPLEXER
  #define pinMultiplexer 16 // "D0"; Pin des Multiplexers
#endif
#if MODUL_WIFI
  #define wifiAdminPasswort "admin" // Passwort für das Admininterface
  #define wifiHostname "pflanzensensor" // Das Gerät ist später unter diesem Name + .local erreichbar
  #define wifiSsid "Magrathea" // WLAN Name
  #define wifiPassword "Gemeinschaftskueche" // WLAN Passwort
  #define wifiIftttPasswort "IFTTT Schlüssel"
  #define wifiIftttEreignis "Fabmobil_Pflanzensensor"
#endif

/**
 * Setup der einzelnen Module
 */
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
  #define displayBreite 128 // Breite des OLED-Displays in Pixeln
  #define displayHoehe 64 // Hoehe des OLED-Displays in Pixeln
  #define displayReset -1 // Display wird mit Arduino Reset Pin zurückgesetzt, wir haben keinen Restknopf..
  #define displayAdresse 0x3C // I2C Adresse des Displays
  #include "modul_display.h"
#endif

#if MODUL_LEDAMPEL
  #include "modul_ledampel.h"
#endif

#if MODUL_HELLIGKEIT
  int messwertHelligkeit;
  int messwertHelligkeitProzent;
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
