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
#include "logger.h"


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
    logger.error("Fehler: Display konnte nicht geöffnet werden.");
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

/**
 * @brief Zeigt das Fabmobil-Logo auf dem Display an
 *
 * Diese Funktion löscht zunächst das Display, zeichnet dann das Fabmobil-Logo
 * und zeigt die Versionsnummer des Pflanzensensors an.
 */
void ZeigeFabmobilLogo() {
  display.clearDisplay();
  display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
  display.setCursor(0, 56);
  display.setTextSize(1);
  display.println(F("v") + String(pflanzensensorVersion));
  display.setTextSize(2);
  display.display();
}

/**
 * @brief Zeigt ein Blumenbild auf dem Display an
 *
 * Diese Funktion löscht das Display und zeichnet dann ein vorgegebenes Blumenbild.
 */
void ZeigeBlume() {
  display.clearDisplay();
  display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
  display.display();
}

/**
 * @brief Zeigt die IP-Adresse und WLAN-Informationen auf dem Display an
 *
 * Diese Funktion löscht zunächst das Display und zeigt dann die WLAN-Informationen an.
 * Je nachdem, ob der Access Point (AP) Modus oder der normale WLAN-Modus aktiv ist,
 * werden unterschiedliche Informationen angezeigt.
 */
void ZeigeIPAdresse() {
    // Löscht den gesamten Displayinhalt
    display.clearDisplay();

    // Setzt die Textgröße auf 2 (größer) und positioniert den Cursor
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("WLAN"); // Überschrift

    // Setzt die Textgröße zurück auf 1 (kleiner) für den restlichen Text
    display.setTextSize(1);

    // Zeigt die IP-Adresse an
    display.setCursor(0, 20);
    display.println(F("IP: ") + ip);

    // Positioniert den Cursor für die nächste Zeile
    display.setCursor(0, 30);

    if (wifiAp) {
        // Wenn der AP-Modus aktiv ist
        display.println(F("AP-Modus"));

        // Zeigt das Passwort an, wenn der AP-Modus aktiv ist
        display.setCursor(0, 40);
        if (wifiApPasswortAktiviert) {
            display.println(F("SSID: ") + String(wifiApSsid) +F(", PW: ") + String(wifiApPasswort));
        } else {
            display.println(F("SSID: ") + String(wifiApSsid) +F(", PW: -keins-"));
        }
    } else {
        // Wenn der normale WLAN-Modus aktiv ist
        display.println(F("WLAN-Modus"));
        display.setCursor(0, 40);
        display.println(F("SSID: ") + aktuelleSSID);
    }

    // Aktualisiert das Display, um die Änderungen anzuzeigen
    display.display();
}

/**
 * @brief Zeigt den Bodenfeuchte-Messwert auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Bodenfeuchte-Messwert
 * anzuzeigen, falls das MODUL_BODENFEUCHTE aktiviert ist.
 */
void ZeigeBodenfeuchte() {
  #if MODUL_BODENFEUCHTE
    MesswertAnzeigen(bodenfeuchteName, "", bodenfeuchteMesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Helligkeits-Messwert auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Helligkeits-Messwert
 * anzuzeigen, falls das MODUL_HELLIGKEIT aktiviert ist.
 */
void ZeigeHelligkeit() {
  #if MODUL_HELLIGKEIT
    MesswertAnzeigen(helligkeitName, "", helligkeitMesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Lufttemperatur-Messwert auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Lufttemperatur-Messwert
 * anzuzeigen, falls das MODUL_DHT aktiviert ist.
 */
void ZeigeLufttemperatur() {
  #if MODUL_DHT
    MesswertAnzeigen("Luft-", "temperatur", lufttemperaturMesswert, "\xf8 C");
  #endif
}

/**
 * @brief Zeigt den Luftfeuchte-Messwert auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Luftfeuchte-Messwert
 * anzuzeigen, falls das MODUL_DHT aktiviert ist.
 */
void ZeigeLuftfeuchte() {
  #if MODUL_DHT
    MesswertAnzeigen("Luft-", "feuchte", luftfeuchteMesswert, "%");
  #endif
}

/**
 * @brief Zeigt den Messwert des Analogsensors 3 auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Messwert des Analogsensors 3
 * anzuzeigen, falls das MODUL_ANALOG3 aktiviert ist.
 */
void ZeigeAnalog3() {
  #if MODUL_ANALOG3
    MesswertAnzeigen(analog3Name, "", analog3MesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Messwert des Analogsensors 4 auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Messwert des Analogsensors 4
 * anzuzeigen, falls das MODUL_ANALOG4 aktiviert ist.
 */
void ZeigeAnalog4() {
  #if MODUL_ANALOG4
    MesswertAnzeigen(analog4Name, "", analog4MesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Messwert des Analogsensors 5 auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Messwert des Analogsensors 5
 * anzuzeigen, falls das MODUL_ANALOG5 aktiviert ist.
 */
void ZeigeAnalog5() {
  #if MODUL_ANALOG5
    MesswertAnzeigen(analog5Name, "", analog5MesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Messwert des Analogsensors 6 auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Messwert des Analogsensors 6
 * anzuzeigen, falls das MODUL_ANALOG6 aktiviert ist.
 */
void ZeigeAnalog6() {
  #if MODUL_ANALOG6
    MesswertAnzeigen(analog6Name, "", analog6MesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Messwert des Analogsensors 7 auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Messwert des Analogsensors 7
 * anzuzeigen, falls das MODUL_ANALOG7 aktiviert ist.
 */
void ZeigeAnalog7() {
  #if MODUL_ANALOG7
    MesswertAnzeigen(analog7Name, "", analog7MesswertProzent, "%");
  #endif
}

/**
 * @brief Zeigt den Messwert des Analogsensors 8 auf dem Display an
 *
 * Diese Funktion ruft MesswertAnzeigen() auf, um den Messwert des Analogsensors 8
 * anzuzeigen, falls das MODUL_ANALOG8 aktiviert ist.
 */
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
