/**
 * @file display.h
 * @brief Display Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Steuerung und Anzeige von Informationen
 * auf dem OLED-Display des Pflanzensensors.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <SPI.h> // SPI Library
#include <Wire.h> // Wire Library
#include <Adafruit_GFX.h> // Adafruit GFX Library
#include <Adafruit_SSD1306.h> // Adafruit SSD1306 Library

Adafruit_SSD1306 display(displayBreite, displayHoehe, &Wire, displayReset); // Initialisierung des Displays

#include "display_bilder.h" // Bilder fürs Display


/**
 * @brief Zeigt die aktuelle IP-Adresse auf dem Display an
 *
 * Diese Funktion löscht das Display und zeigt dann die aktuelle IP-Adresse,
 * den WLAN-Modus (AP oder Client) und die SSID an.
 */
void DisplayIPAdresse() {
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


/**
 * @brief Teilt einen Namen in zwei Teile, falls er länger als 10 Zeichen ist
 *
 * @param name Der zu teilende Name
 * @return std::pair<String, String> Ein Paar mit den beiden Teilen des Namens
 */
std::pair<String, String> NamenTeilen(String name) {
  int laenge = name.length();
  if (laenge >= 10) {
    int mitte = laenge / 2;
    String teil1 = name.substring(0, mitte);
    teil1 += "-";
    String teil2 = name.substring(mitte, laenge);
    return std::make_pair(teil1, teil2);
  } else {
    return std::make_pair(name, "");
  }
}

/**
 * @brief Zeigt einen Messwert auf dem Display an
 *
 * @param name1 Erster Teil des Sensornamens
 * @param name2 Zweiter Teil des Sensornamens (optional)
 * @param messwert Der anzuzeigende Messwert
 * @param einheit Die Einheit des Messwerts
 */
void MesswertAnzeigen(String name1, String name2, int messwert, String einheit){
  display.clearDisplay(); // Display löschen
  display.setCursor(40, 0);
  display.println(messwert);
  display.setCursor(80, 0);
  display.println(einheit);
  display.setCursor(0, 20);
  display.println(name1);
  display.setCursor(5, 40);
  display.println(name2);
  display.display();      // Display aktualisieren
}

/**
 * @brief Spielt den Bootscreen auf dem Display ab und zeigt die IP-Adresse an
 *
 * @param ip Die anzuzeigende IP-Adresse
 * @param hostname Der Hostname des Geräts
 */
void DisplayIntro(String ip, String hostname) {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von DisplayIntro()"));
  #endif
  display.clearDisplay(); // Display löschen

  display.setTextSize(2); // Doppelt großer Text
  display.setTextColor(SSD1306_WHITE); // Weißer Text
  display.setCursor(0, 0); // Cursor auf 0,0 setzen
  display.println(F("FABMOBIL")); // Text ausgeben
  display.display();      // Display aktualisieren
  delay(100); // 100 Millisekunden warten

  // in verschiedene Richtungen scrollen:
  display.startscrollright(0x00, 0x06);
  delay(2000);
  display.stopscroll();
  display.startscrollleft(0x00, 0x06);
  delay(2000);
  display.stopscroll();
  delay(500);
  display.setCursor(10, 20);
  display.println(F("Pflanzen-"));
  display.display();      // Display aktualisieren
  delay(200);
  display.setCursor(20, 40);
  display.println(F("sensor"));
  display.display();      // Display aktualisieren
  delay(500);
  display.setTextSize(1);
  display.setCursor(95, 54);
  display.println(pflanzensensorVersion);
  display.display();      // Display aktualisieren
  delay(2000);

  display.clearDisplay(); // Display löschen
  display.setCursor(0, 0); // Cursor auf 0,0 setzen
  display.setTextSize(2); // Doppelt großer Text
  display.println(F("IP Adresse")); // Text ausgeben
  display.setTextSize(1); // Normaler Text
  display.setCursor(0, 17); // Cursor auf 0,17 setzen
  display.println(ip); // IP Adresse ausgeben
  display.setCursor(0, 35); // Cursor auf 0,35 setzen
  display.setTextSize(2);   // Doppelt großer Text
  display.println(F("Hostname")); // Text ausgeben
  display.setTextSize(1);   // Normaler Text
  display.setCursor(0, 52); // Cursor auf 0,52 setzen
  display.print(hostname); // Hostname ausgeben
  display.println(".local"); // .local anhängen
  display.display();     // Display aktualisieren
  delay(5000);

}

/**
 * @brief Zeigt die aktuellen Messwerte auf dem Display an
 *
 * Diese Funktion aktualisiert das Display mit den aktuellen Messwerten
 * aller aktivierten Sensoren.
 */
void DisplayAnzeigen() {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von DisplayAnzeigen(")); Serial.print(helligkeitMesswertProzent);
    Serial.print(F(", ")); Serial.print(luftfeuchteMesswert);
    Serial.print(F(", ")); Serial.print(lufttemperaturMesswert);
    Serial.print(F(", ")); Serial.print(status);
    Serial.println(F(")"));
  #endif

  display.clearDisplay();
  display.setTextSize(2);

  switch (status) {
    case 0:
      display.clearDisplay();
      display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
      display.display();
      #if MODUL_LEDAMPEL
        if (ampelAn && ampelModus == 1) { LedampelAus(); }
      #endif
      break;
    case 1:
      display.clearDisplay();
      display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
      display.display();
      #if MODUL_LEDAMPEL
        if (ampelAn && ampelModus == 1) { LedampelAus(); }
      #endif
      break;
    case 2:
      if (bodenfeuchteMesswertProzent != -1) {
        std::pair<String, String> namen = NamenTeilen(bodenfeuchteName);
        MesswertAnzeigen(namen.first, namen.second, bodenfeuchteMesswertProzent, "%");
        #if MODUL_LEDAMPEL
          if (ampelAn && ampelModus == 1) { LedampelAnzeigen(bodenfeuchteFarbe, -1); }
        #endif
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 3:
      if (helligkeitMesswertProzent != -1) {
        std::pair<String, String> namen = NamenTeilen(helligkeitName);
        MesswertAnzeigen(namen.first, namen.second, helligkeitMesswertProzent, "%");
        #if MODUL_LEDAMPEL
          if (ampelAn && ampelModus == 1) { LedampelAnzeigen(helligkeitFarbe, -1); }
        #endif
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 4:
      if (lufttemperaturMesswert != -1) {
        MesswertAnzeigen("Luft-", "temperatur", lufttemperaturMesswert, "\xf8 C");
        #if MODUL_LEDAMPEL
          if (ampelAn && ampelModus == 1) { LedampelAnzeigen(lufttemperaturFarbe, -1); }
        #endif
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 5:
      if (luftfeuchteMesswert != -1) {
        MesswertAnzeigen("Luft-", "feuchte", luftfeuchteMesswert, "%");
        #if MODUL_LEDAMPEL
          if (ampelAn && ampelModus == 1) { LedampelAnzeigen(luftfeuchteFarbe, -1); }
        #endif
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 6:
      DisplayIPAdresse();
      #if MODUL_LEDAMPEL
        if (ampelAn && ampelModus == 1) { LedampelAus(); }
      #endif
      break;
    #if MODUL_ANALOG3
      case 7:
        if (analog3MesswertProzent != -1) {
          std::pair<String, String> namen = NamenTeilen(analog3Name);
          MesswertAnzeigen(namen.first, namen.second, analog3MesswertProzent, "%");
          #if MODUL_LEDAMPEL
            if (ampelAn && ampelModus == 1) { LedampelAnzeigen(analog3Farbe, -1); }
          #endif
        } else {
          display.clearDisplay();
          display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
          display.display();
        }
        break;
    #endif
    #if MODUL_ANALOG4
      case 8:
        if (analog4MesswertProzent != -1) {
          std::pair<String, String> namen = NamenTeilen(analog4Name);
          MesswertAnzeigen(namen.first, namen.second, analog4MesswertProzent, "%");
          #if MODUL_LEDAMPEL
            if (ampelAn && ampelModus == 1) { LedampelAnzeigen(analog4Farbe, -1); }
          #endif
        } else {
          display.clearDisplay();
          display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
          display.display();
        }
        break;
    #endif
    #if MODUL_ANALOG5
      case 9:
        if (analog5MesswertProzent != -1) {
          std::pair<String, String> namen = NamenTeilen(analog5Name);
          MesswertAnzeigen(namen.first, namen.second, analog5MesswertProzent, "%");
          #if MODUL_LEDAMPEL
            if (ampelAn && ampelModus == 1) { LedampelAnzeigen(analog5Farbe, -1); }
          #endif
        } else {
          display.clearDisplay();
          display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
          display.display();
        }
        break;
    #endif
    #if MODUL_ANALOG6
      case 10:
        if (analog6MesswertProzent != -1) {
          std::pair<String, String> namen = NamenTeilen(analog6Name);
          MesswertAnzeigen(namen.first, namen.second, analog6MesswertProzent, "%");
          #if MODUL_LEDAMPEL
            if (ampelAn && ampelModus == 1) { LedampelAnzeigen(analog6Farbe, -1); }
          #endif
        } else {
          display.clearDisplay();
          display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
          display.display();
        }
        break;
    #endif
    #if MODUL_ANALOG7
      case 11:
        if (analog7MesswertProzent != -1) {
          std::pair<String, String> namen = NamenTeilen(analog7Name);
          MesswertAnzeigen(namen.first, namen.second, analog7MesswertProzent, "%");
          #if MODUL_LEDAMPEL
            if (ampelAn && ampelModus == 1) { LedampelAnzeigen(analog7Farbe, -1); }
          #endif
        } else {
          display.clearDisplay();
          display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
          display.display();
        }
        break;
    #endif
    #if MODUL_ANALOG8
      case 12:
        if (analog8MesswertProzent != -1) {
          std::pair<String, String> namen = NamenTeilen(analog8Name);
          MesswertAnzeigen(namen.first, namen.second, analog8MesswertProzent, "%");
          #if MODUL_LEDAMPEL
            if (ampelAn && ampelModus == 1) { LedampelAnzeigen(analog8Farbe, -1); }
          #endif
        } else {
          display.clearDisplay();
          display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
          display.display();
        }
        break;
    #endif
  }
}

/**
 * @brief Zeigt drei Wörter auf dem Display an
 *
 * @param wort1 Erstes Wort
 * @param wort2 Zweites Wort
 * @param wort3 Drittes Wort
 */
void DisplayDreiWoerter(String wort1, String wort2, String wort3) {
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

void DisplayAus() {
  display.clearDisplay();
  display.display();
}

/**
 * @brief Initialisiert das Display
 *
 * Diese Funktion richtet das Display ein und zeigt den Intro-Bildschirm an.
 */
void DisplaySetup() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von DisplaySetup()"));
  #endif
  // hier wird überprüft, ob die Verbindung zum Display erfolgreich war:
  if(!display.begin(SSD1306_SWITCHCAPVCC, displayAdresse)) {
    Serial.println(F("Fehler: Display konnte nicht geöffnet werden."));
  }
  display.display(); // Display anschalten und initialen Buffer zeigen
  delay(100); // 1 Sekunde warten
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.clearDisplay(); // Display löschen
  // DisplayIntro(ip, wifiHostname); // Intro auf Display abspielen
  DisplayDreiWoerter("Start..", " bitte", " warten!");
}

#endif // DISPLAY_H
