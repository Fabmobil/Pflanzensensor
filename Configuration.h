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
#define MODUL_DEBUG         true  // Debugmodus (de)aktivieren
#define MODUL_DISPLAY       true  // hat dein Pflanzensensor ein Display?
#define MODUL_WIFI          true // verwendet dein Pflanzensensor das WiFi-Modul?
#define MODUL_DHT           true // hat dein Pflanzensensor ein Luftfeuchte- und Temperaturmesser?
#define MODUL_BODENFEUCHTE  false // hat dein Pflanzensensor einen Bodenfeuchtemesser?
#define MODUL_LEDAMPEL      true // hat dein Pflanzensensor eine LED Ampel?
#define MODUL_HELLIGKEIT   true // hat dein Pflanzensensor einen Lichtsensor?
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
#define baudrateSeriell 9600 // Baudrate der seriellen Verbindung
#if MODUL_BODENFEUCHTE || MODUL_HELLIGKEIT
  #define pinAnalog A0
#endif
#if MODUL_DHT
  #define pinDht 0 // "D3"; an welchem Pin ist der Sensor angeschlossen?
  #define dhtSensortyp DHT11  // ist ein DHT11 (blau) oder ein DHT22 (weiss) Sensor verbaut?
#endif
#if MODUL_DISPLAY
  #define displayAnzeigedauer 1500 // Anzeigedauer der einzelnen Messwerte auf dem Display in Millisekunden
#endif
#if MODUL_LEDAMPEL
  #define pinAmpelRot 12 // Pin der roten LED
  #define pinAmpelGelb 13 // Pin der gelben LED
  #define pinAmpelGruen 14 // Pin der gruenen LED
  #define ampelBodenfeuchteInvertiert false // true: grün = großer Wert, rot = kleiner Wert. false: rot = großer Wert, grün = kleiner Wert
  #define ampelBodenfeuchteGruen 40 // Luftfeuchte in %, ab der die Ampel grün ist
  #define ampelBodenfeuchteGelb 30 // Luftfeuchte in %, ab der die Ampel gelb und unter der die Ampel rot ist
  #define ampelBodenfeuchteRot 40 // Luftfeuchte in %, ab der die Ampel grün ist
  #define ampelLichtstaerkeInvertiert false // true: grün = klein, rot = groß. false: rot = klein, grün = groß
  #define ampelLichtstaerkeGruen 50 // Lichtstärke in %, bis zu der Ampel grün ist
  #define ampelLichtstaerkeGelb 30 // Lichtstärke in %, bis zu der Ampel grün ist
  #define ampelLichtstaerkeRot 10 // Lichtstärke in %, bis zu der Ampel grün ist
#endif
#if MODUL_HELLIGKEIT
  const int lichtstaerkeMinimum = 1024;
  const int lichtstaerkeMaximum = 0;
  //#define lichtstaerkeMinimum 1024
  //#define lichtstaerkeMaximum 0
#endif
#if MODUL_MULTIPLEXER
  #define pinMultiplexer1 8 // "S1"; erster Eingangspin des Multiplexers
  #define pinMultiplexer2 9 // "S2"; zweiter Eingangspin des Multiplexers
  #define pinMultiplexer3 10 // "S3"; dritter Eingangspin des Multiplexers
#endif
#if MODUL_WIFI
  #define wifiSsid "Tommy" // WLAN Name
  #define wifiPassword "freibier" // WLAN Passwort
  #define wifiIftttPasswort "IFTTT Schlüssel"
  #define wifiIftttEreignis "Fabmobil_Pflanzensensor"
#endif 

/**
 * Setup der einzelnen Module
 */
#if MODUL_BODENFEUCHTE
  #include "modul_bodenfeuchte.h"
#else
  #define messwertBodenfeuchte -1
#endif
 
#if MODUL_DHT
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
  #include "modul_helligkeit.h"
#else
  #define messwertHelligkeit -1
#endif

#if MODUL_MULTIPLEXER
  #include "modul_multiplexer.h"
#endif

#if MODUL_WIFI
  #include "modul_wifi.h"
#endif
