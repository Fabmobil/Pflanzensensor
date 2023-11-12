/**
 * Fabmobil Pflanzensensor Konfigurationsdatei
 * 
 * hier werden die Module ausgewählt sowie die PINs und
 * Variablen festgelegt.
 */

/**
 * Module 
 * "true" aktiviert sie, "false" deaktiviert sie
 * 
 */
#define MODUL_DEBUG         true  // Debugmodus (de)aktivieren
#define MODUL_DISPLAY       true  // hat dein Pflanzensensor ein Display?
#define MODUL_WIFI          true // verwendet dein Pflanzensensor das WiFi-Modul?
#define MODUL_DHT           true // hat dein Pflanzensensor ein Luftfeuchte- und Temperaturmesser?
#define MODUL_BODENFEUCHTE  false // hat dein Pflanzensensor einen Bodenfeuchtemesser?
#define MODUL_LEDAMPEL      true // hat dein Pflanzensensor eine LED Ampel?
#define MODUL_LICHTSENSOR   true // hat dein Pflanzensensor einen Lichtsensor?
// Wenn Bodenfeuchte- und Lichtsensor verwendet werden, brauchen wir auch einen Analog-Multiplexer:
#if MODUL_BODENFEUCHTE && MODUL_LICHTSENSOR
  #define MODUL_MULTIPLEXER true 
#else
  #define MODUL_MULTIPLEXER false
#endif

/**
 * Pinbelegungen und Variablen
 */
#define PIN_EINGEBAUTELED LED_BUILTIN // "D0"; worüber wird die interne LED des ESPs angesprochen?
#define VAR_BAUDRATE 9600 // Baudrate der seriellen Verbindung
#if MODUL_BODENFEUCHTE || MODUL_LICHTSENSOR
  #define PIN_ANALOG A0
#endif
#if MODUL_DHT
  #define PIN_DHT 0 // "D3"; an welchem Pin ist der Sensor angeschlossen?
  #define VAR_DHT_SENSOR_TYPE DHT11  // ist ein DHT11 (blau) oder ein DHT22 (weiss) Sensor verbaut?
#endif
#if MODUL_DISPLAY
  #define VAR_DISPLAY_ANZEIGEDAUER 1500 // Anzeigedauer der einzelnen Messwerte auf dem Display in Millisekunden
#endif
#if MODUL_LEDAMPEL
  #define PIN_LEDAMPEL_ROTELED 12 // Pin der roten LED
  #define PIN_LEDAMPEL_GELBELED 13 // Pin der gelben LED
  #define PIN_LEDAMPEL_GRUENELED 14 // Pin der gruenen LED
  #define VAR_LEDAMPEL_BODENFEUCHTE_INVERTIERT false // true: grün = großer Wert, rot = kleiner Wert. false: rot = großer Wert, grün = kleiner Wert
  #define VAR_LEDAMPEL_BODENFEUCHTE_GRUEN 40 // Luftfeuchte in %, ab der die Ampel grün ist
  #define VAR_LEDAMPEL_BODENFEUCHTE_GELB 30 // Luftfeuchte in %, ab der die Ampel gelb und unter der die Ampel rot ist
  #define VAR_LEDAMPEL_BODENFEUCHTE_ROT 40 // Luftfeuchte in %, ab der die Ampel grün ist
  #define VAR_LEDAMPEL_LICHTSTAERKE_INVERTIERT false // true: grün = klein, rot = groß. false: rot = klein, grün = groß
  #define VAR_LEDAMPEL_LICHTSTAERKE_GRUEN 50 // Lichtstärke in %, bis zu der Ampel grün ist
  #define VAR_LEDAMPEL_LICHTSTAERKE_GELB 30 // Lichtstärke in %, bis zu der Ampel grün ist
  #define VAR_LEDAMPEL_LICHTSTAERKE_ROT 10 // Lichtstärke in %, bis zu der Ampel grün ist
#endif
#if MODUL_LICHTSENSOR
  const int VAR_LICHTSTAERKE_MIN = 1024;
  const int VAR_LICHTSTAERKE_MAX = 0;
  //#define VAR_LICHTSTAERKE_MIN 1024
  //#define VAR_LICHTSTAERKE_MAX 0
#endif
#if MODUL_MULTIPLEXER
  #define PIN_MULTIPLEXER_1 8 // "S1"; erster Eingangspin des Multiplexers
  #define PIN_MULTIPLEXER_2 9 // "S2"; zweiter Eingangspin des Multiplexers
  #define PIN_MULTIPLEXER_3 10 // "S3"; dritter Eingangspin des Multiplexers
#endif
#if MODUL_WIFI
  #define VAR_WIFI_SSID "Tommy" // WLAN Name
  #define VAR_WIFI_PASSWORD "freibier" // WLAN Passwort
  #define VAR_WIFI_IFTTT_PASSWORT "IFTTT Schlüssel"
  #define VAR_WIFI_IFTTT_EREIGNIS "Fabmobil_Pflanzensensor"
#endif 

/**
 * Setup der einzelnen Module
 */
 
#if MODUL_BODENFEUCHTE
  #include "modul_bodenfeuchte.h"
#else
  #define MESSWERT_BODENFEUCHTE -1
#endif
 
#if MODUL_DHT
  #include "modul_dht.h"
#else
  #define MESSWERT_LUFTTEMPERATUR -1
  #define MESSWERT_LUFTFEUCHTE -1
#endif

#if MODUL_DISPLAY
  #include <SPI.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #define VAR_DISPLAY_BREITE 128 // Breite des OLED-Displays in Pixeln
  #define VAR_DISPLAY_HOEHE 64 // Hoehe des OLED-Displays in Pixeln
  #define VAR_DISPLAY_RESET -1 // Display wird mit Arduino Reset Pin zurückgesetzt, wir haben keinen Restknopf..
  #define VAR_DISPLAY_ADRESSE 0x3C // I2C Adresse des Displays
  Adafruit_SSD1306 display(VAR_DISPLAY_BREITE, VAR_DISPLAY_HOEHE, &Wire, VAR_DISPLAY_RESET); // Initialisierung des Displays
  #include "modul_display.h"
#endif

#if MODUL_LEDAMPEL
  #include "modul_ledampel.h"
#endif

#if MODUL_LICHTSENSOR
  #include "modul_lichtsensor.h"
#else
  #define LICHTSTAERKE_MESSWERT -1
#endif

#if MODUL_MULTIPLEXER
  #include "modul_multiplexer.h"
#endif

#if MODUL_WIFI
  #include "modul_wifi.h"
#endif

int LICHTSTAERKE_MESSWERT = -1;
