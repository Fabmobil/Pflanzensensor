/**
 * @file display.cpp
 * @brief Implementierung des Display Moduls für den Pflanzensensor
 * @author Tommy, Claude
 * @date 2023-09-20
 */

#include "einstellungen.h"
#include "passwoerter.h"

#include "display.h"
#include "display_bilder.h"
#include "logger.h"
#include "ledampel.h"

Adafruit_SSD1306 display(displayBreite, displayHoehe, &Wire, displayReset);

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

void NaechsteSeite() {
  do {
    aktuelleSeite = (aktuelleSeite + 1) % (sizeof(displayseiten) / sizeof(displayseiten[0]));
  } while (!displayseiten[aktuelleSeite].istAktiv);
}

void DisplaySetup() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, displayAdresse)) {
    logger.error(F("Fehler: Display konnte nicht geöffnet werden."));
    return;
  }

  display.display();
  delay(100);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.clearDisplay();

  DisplayDreiWoerter(F("Start.."), F(" bitte"), F(" warten!"));
}

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
#if MODUL_WIFI
    // Löscht den gesamten Displayinhalt
    display.clearDisplay();

    // Setzt die Textgröße auf 2 (größer) und positioniert den Cursor
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println(F("WLAN")); // Überschrift

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
        display.println(F("SSID: ") + aktuelleSsid);
    }

    // Aktualisiert das Display, um die Änderungen anzuzeigen
    display.display();
#endif
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
    MesswertAnzeigen(F("Luft-"), F("temperatur"), lufttemperaturMesswert,F("\xf8 C"));
  #endif
}

void ZeigeLuftfeuchte() {
  #if MODUL_DHT
    MesswertAnzeigen(F("Luft-"), F("feuchte"), luftfeuchteMesswert, F("%"));
  #endif
}

void ZeigeAnalog3() {
  #if MODUL_ANALOG3
    MesswertAnzeigen(analog3Name, "", analog3MesswertProzent, F("%"));
  #endif
}

void ZeigeAnalog4() {
  #if MODUL_ANALOG4
    MesswertAnzeigen(analog4Name, "", analog4MesswertProzent, F("%"));
  #endif
}

void ZeigeAnalog5() {
  #if MODUL_ANALOG5
    MesswertAnzeigen(analog5Name, "", analog5MesswertProzent, F("%"));
  #endif
}

void ZeigeAnalog6() {
  #if MODUL_ANALOG6
    MesswertAnzeigen(analog6Name, "", analog6MesswertProzent, F("%"));
  #endif
}

void ZeigeAnalog7() {
  #if MODUL_ANALOG7
    MesswertAnzeigen(analog7Name, "", analog7MesswertProzent, F("%"));
  #endif
}

void ZeigeAnalog8() {
  #if MODUL_ANALOG8
    MesswertAnzeigen(analog8Name, "", analog8MesswertProzent, F("%"));
  #endif
}

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
