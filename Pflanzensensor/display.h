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

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(displayBreite, displayHoehe, &Wire, displayReset);

#include "display_bilder.h"

// Vorwärtsdeklarationen
void ZeigeFabmobilLogo();
void ZeigeBlume();
void ZeigeIPAdresse();
void ZeigeBodenfeuchte();
void ZeigeHelligkeit();
void ZeigeLufttemperatur();
void ZeigeLuftfeuchte();
void ZeigeAnalog3();
void ZeigeAnalog4();
void ZeigeAnalog5();
void ZeigeAnalog6();
void ZeigeAnalog7();
void ZeigeAnalog8();
void DisplayDreiWoerter(const String& wort1, const String& wort2, const String& wort3);
void DisplaySechsZeilen(String zeile1, String zeile2, String zeile3, String zeile4, String zeile5, String zeile6);
void MesswertAnzeigen(const String& name1, const String& name2, int messwert, const String& einheit);

// Struktur für Displayseiten
typedef struct {
  void (*anzeigeFunktion)();
  bool istAktiv;
  String* farbe;  // Zeiger auf die Farbvariable
} Displayseite;

// Array von Displayseiten
Displayseite displayseiten[] = {
  {ZeigeFabmobilLogo, true, nullptr},
  {ZeigeBlume, true, nullptr},
  {ZeigeBodenfeuchte, MODUL_BODENFEUCHTE, &bodenfeuchteFarbe},
  {ZeigeHelligkeit, MODUL_HELLIGKEIT, &helligkeitFarbe},
  {ZeigeLufttemperatur, MODUL_DHT, &lufttemperaturFarbe},
  {ZeigeLuftfeuchte, MODUL_DHT, &luftfeuchteFarbe},
  {ZeigeIPAdresse, true, nullptr},
  {ZeigeAnalog3, MODUL_ANALOG3, &analog3Farbe},
  {ZeigeAnalog4, MODUL_ANALOG4, &analog4Farbe},
  {ZeigeAnalog5, MODUL_ANALOG5, &analog5Farbe},
  {ZeigeAnalog6, MODUL_ANALOG6, &analog6Farbe},
  {ZeigeAnalog7, MODUL_ANALOG7, &analog7Farbe},
  {ZeigeAnalog8, MODUL_ANALOG8, &analog8Farbe}
};

int aktuelleSeite = 0;

/**
 * @brief Zeigt die aktuelle Seite auf dem Display an
 */
void DisplayAnzeigen() {
  if (!displayAn) {
    display.clearDisplay();
    display.display();
    return;
  }

  // Aktuelle Seite anzeigen
  displayseiten[aktuelleSeite].anzeigeFunktion();

  // Aktualisiere die LED-Ampel, wenn aktiviert
  #if MODUL_LEDAMPEL
    if (ampelAn && ampelModus == 1) {
      if (displayseiten[aktuelleSeite].farbe != nullptr) {
        LedampelAnzeigen(*displayseiten[aktuelleSeite].farbe, -1);
      } else {
        LedampelAus();
      }
    }
  #endif
}

/**
 * @brief Wechselt zur nächsten aktiven Displayseite
 */
void NaechsteSeite() {
  do {
    aktuelleSeite = (aktuelleSeite + 1) % (sizeof(displayseiten) / sizeof(displayseiten[0]));
  } while (!displayseiten[aktuelleSeite].istAktiv);
}

/**
 * @brief Initialisiert das Display und zählt die aktiven Seiten
 */
void DisplaySetup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, displayAdresse)) {
    Serial.println(F("Fehler: Display konnte nicht geöffnet werden."));
    return;
  }

  display.display();
  delay(100);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.clearDisplay();

  DisplayDreiWoerter("Start..", " bitte", " warten!");
}

// Implementierung der einzelnen Anzeigeseiten

void ZeigeFabmobilLogo() {
  display.clearDisplay();
  display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
  display.setCursor(0, 56);
  display.setTextSize(1);
  display.println(F("v") + String(pflanzensensorVersion));
  display.setTextSize(2);
  display.display();
}

void ZeigeBlume() {
  display.clearDisplay();
  display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
  display.display();
}

void ZeigeIPAdresse() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("WLAN");
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(F("IP: ") + ip);
  display.setCursor(0, 30);
  if (wifiAp) {
    display.println(F("AP-Modus"));
    display.setCursor(0, 40);
    display.println(F("SSID: ") + String(wifiApSsid));
  } else {
    display.println(F("WLAN-Modus"));
    display.setCursor(0, 40);
    display.println(F("SSID: ") + aktuelleSSID);
  }
  display.display();
}

void ZeigeBodenfeuchte() {
  #if MODUL_BODENFEUCHTE
    MesswertAnzeigen(bodenfeuchteName, "", bodenfeuchteMesswertProzent, "%");
  #endif
}

void ZeigeHelligkeit() {
  #if MODUL_HELLIGKEIT
    MesswertAnzeigen(helligkeitName, "", helligkeitMesswertProzent, "%");
  #endif
}

void ZeigeLufttemperatur() {
  #if MODUL_DHT
    MesswertAnzeigen("Luft-", "temperatur", lufttemperaturMesswert, "\xf8 C");
  #endif
}

void ZeigeLuftfeuchte() {
  #if MODUL_DHT
    MesswertAnzeigen("Luft-", "feuchte", luftfeuchteMesswert, "%");
  #endif
}

void ZeigeAnalog3() {
  #if MODUL_ANALOG3
    MesswertAnzeigen(analog3Name, "", analog3MesswertProzent, "%");
  #endif
}

void ZeigeAnalog4() {
  #if MODUL_ANALOG4
    MesswertAnzeigen(analog4Name, "", analog4MesswertProzent, "%");
  #endif
}

void ZeigeAnalog5() {
  #if MODUL_ANALOG5
    MesswertAnzeigen(analog5Name, "", analog5MesswertProzent, "%");
  #endif
}

void ZeigeAnalog6() {
  #if MODUL_ANALOG6
    MesswertAnzeigen(analog6Name, "", analog6MesswertProzent, "%");
  #endif
}

void ZeigeAnalog7() {
  #if MODUL_ANALOG7
    MesswertAnzeigen(analog7Name, "", analog7MesswertProzent, "%");
  #endif
}

void ZeigeAnalog8() {
  #if MODUL_ANALOG8
    MesswertAnzeigen(analog8Name, "", analog8MesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt drei Wörter auf dem Display an
 *
 * @param wort1 Erstes Wort
 * @param wort2 Zweites Wort
 * @param wort3 Drittes Wort
 */
void DisplayDreiWoerter(const String& wort1, const String& wort2, const String& wort3) {
  display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(wort1);
  display.setCursor(0, 20);
  display.println(wort2);
  display.setCursor(0, 40);
  display.println(wort3);
  display.display();
  delay(1000); // mindestens 1s Zeit zum lesen
}

/**
 * @brief Zeigt sechs Zeilen Text auf dem Display an
 *
 * @param zeile1 Erste Zeile
 * @param zeile2 Zweite Zeile
 * @param zeile3 Dritte Zeile
 * @param zeile4 Vierte Zeile
 * @param zeile5 Fünfte Zeile
 * @param zeile6 Sechste Zeile
 */
void DisplaySechsZeilen(String zeile1, String zeile2, String zeile3, String zeile4, String zeile5, String zeile6) {
  display.setTextSize(1);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(zeile1);
  display.setCursor(0, 10);
  display.println(zeile2);
  display.setCursor(0, 20);
  display.println(zeile3);
  display.setCursor(0, 30);
  display.println(zeile4);
  display.setCursor(0, 40);
  display.println(zeile5);
  display.setCursor(0, 50);
  display.println(zeile6);
  display.display();
  delay(3000); // mindestens 1s Zeit zum lesen
}

/**
 * @brief Zeigt einen Messwert auf dem Display an
 *
 * @param name1 Erster Teil des Sensornamens
 * @param name2 Zweiter Teil des Sensornamens (optional)
 * @param messwert Der anzuzeigende Messwert
 * @param einheit Die Einheit des Messwerts
 */
void MesswertAnzeigen(const String& name1, const String& name2, int messwert, const String& einheit) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(40, 0);
  display.println(messwert);
  display.setCursor(80, 0);
  display.println(einheit);
  display.setCursor(0, 20);
  display.println(name1);
  display.setCursor(5, 40);
  display.println(name2);
  display.display();
}

#endif // DISPLAY_H
