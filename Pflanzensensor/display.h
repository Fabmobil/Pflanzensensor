/**
 * @file display.h
 * @brief Display Modul für den Pflanzensensor
 * @author Tommy, Claude
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Steuerung und Anzeige von Informationen
 * auf dem OLED-Display des Pflanzensensors.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

//#include "einstellungen.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "display.h"
#include "display_bilder.h"
#include "logger.h"
//#include "einstellungen.h"

extern Adafruit_SSD1306 display;
// extern const int displayBreite;
// extern const int displayHoehe;
// extern const int displayReset;
// extern bool displayAn;
// extern const int displayAdresse;
// extern bool wifiAp;
// extern String aktuelleSsid;
// extern String wifiApSsid;
// extern bool wifiApPasswortAktiviert;
// extern String wifiApPasswort;

/**
 * @brief Zeigt das Fabmobil-Logo auf dem Display an
 */
void ZeigeFabmobilLogo();

/**
 * @brief Zeigt ein Blumenbild auf dem Display an
 */
void ZeigeBlume();

/**
 * @brief Zeigt die IP-Adresse und WLAN-Informationen auf dem Display an
 */
void ZeigeIPAdresse();

/**
 * @brief Zeigt den Bodenfeuchte-Messwert auf dem Display an
 */
void ZeigeBodenfeuchte();

/**
 * @brief Zeigt den Helligkeits-Messwert auf dem Display an
 */
void ZeigeHelligkeit();

/**
 * @brief Zeigt den Lufttemperatur-Messwert auf dem Display an
 */
void ZeigeLufttemperatur();

/**
 * @brief Zeigt den Luftfeuchte-Messwert auf dem Display an
 */
void ZeigeLuftfeuchte();

/**
 * @brief Zeigt den Messwert des Analogsensors 3 auf dem Display an
 */
void ZeigeAnalog3();

/**
 * @brief Zeigt den Messwert des Analogsensors 4 auf dem Display an
 */
void ZeigeAnalog4();

/**
 * @brief Zeigt den Messwert des Analogsensors 5 auf dem Display an
 */
void ZeigeAnalog5();

/**
 * @brief Zeigt den Messwert des Analogsensors 6 auf dem Display an
 */
void ZeigeAnalog6();

/**
 * @brief Zeigt den Messwert des Analogsensors 7 auf dem Display an
 */
void ZeigeAnalog7();

/**
 * @brief Zeigt den Messwert des Analogsensors 8 auf dem Display an
 */
void ZeigeAnalog8();

/**
 * @brief Zeigt drei Wörter auf dem Display an
 * @param wort1 Erstes Wort
 * @param wort2 Zweites Wort
 * @param wort3 Drittes Wort
 */
void DisplayDreiWoerter(const String& wort1, const String& wort2, const String& wort3);

/**
 * @brief Zeigt sechs Zeilen Text auf dem Display an
 * @param zeile1 Erste Zeile
 * @param zeile2 Zweite Zeile
 * @param zeile3 Dritte Zeile
 * @param zeile4 Vierte Zeile
 * @param zeile5 Fünfte Zeile
 * @param zeile6 Sechste Zeile
 */
void DisplaySechsZeilen(String zeile1, String zeile2, String zeile3, String zeile4, String zeile5, String zeile6);

/**
 * @brief Zeigt einen Messwert auf dem Display an
 * @param name1 Erster Teil des Sensornamens
 * @param name2 Zweiter Teil des Sensornamens (optional)
 * @param messwert Der anzuzeigende Messwert
 * @param einheit Die Einheit des Messwerts
 */
void MesswertAnzeigen(const String& name1, const String& name2, int messwert, const String& einheit);

// Struktur für Displayseiten
typedef struct {
  void (*anzeigeFunktion)();
  bool istAktiv;
  String* farbe;  // Zeiger auf die Farbvariable
} Displayseite;

extern Displayseite displayseiten[];
extern int aktuelleSeite;

/**
 * @brief Zeigt die aktuelle Seite auf dem Display an
 */
void DisplayAnzeigen();

/**
 * @brief Wechselt zur nächsten aktiven Displayseite
 */
void NaechsteSeite();

/**
 * @brief Initialisiert das Display und zählt die aktiven Seiten
 */
void DisplaySetup();

#endif // DISPLAY_H
