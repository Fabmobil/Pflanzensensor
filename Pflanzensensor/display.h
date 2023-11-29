/**
 * Display Modul
 * Diese Datei enthält den Code für das Display-Modul.
 * Sonderzeichen müssen über einen Code eingegeben werden damit sie
 * richtig angezeigt werden:
 * \x84 -> ä; \x94 -> ö; \x81 -> ü; \x8e -> Ä; \x99 -> Ö;
 * \x9a -> Ü; \xe1 -> ß; \xe0 -> alpha; \xe4 -> Summenzeichen;
 * \xe3 -> Pi; \xea -> Ohm; \xed -> Durchschnitt; \xee -> PI;
 * \x10 -> Pfeil links; \x11 -> Pfeil rechts; \x12 -> Pfeil
 * oben und unten; \x7b -> {; \x7c -> |; \x7d -> };
 * \xf8 -> °
 */

#include <SPI.h> // SPI Library
#include <Wire.h> // Wire Library
#include <Adafruit_GFX.h> // Adafruit GFX Library
#include <Adafruit_SSD1306.h> // Adafruit SSD1306 Library

Adafruit_SSD1306 display(displayBreite, displayHoehe, &Wire, displayReset); // Initialisierung des Displays

#include "display_bilder.h" // Bilder fürs Display


/* Funktion: NamenTeilen(String name)
 * Teilt einen Namen, falls er länger als 10 Zeichen ist, in zwei Teile
 * String name: Name, der geteilt werden soll
 * Rückgabe: std::pair<String, String> mit den beiden Teilen
 */
std::pair<String, String> NamenTeilen(String name) {
  int laenge = name.length();
  if (laenge >= 10) {
    int mitte = laenge / 2;
    String teil1 = name.substring(0, mitte);
    teil1 += "-";
    String teil2 = name.substring(mitte, laenge);
    teil2 += ":";
    return std::make_pair(teil1, teil2);
  } else {
    return std::make_pair(name + ":", "");
  }
}

/* Funktion: MesswertAnzeigen(String name1, String name2, float messwert, String einheit)
 * Zeigt einen Messwert auf dem Display an
 * String name1: Erster Teil des Namens
 * String name2: Zweiter Teil des Namens
 * float messwert: Messwert
 * String einheit: Einheit des Messwertes
 */
void MesswertAnzeigen(String name1, String name2, int messwert, String einheit){
  display.setCursor(0, 0);
  display.println(name1);
  display.setCursor(10, 20);
  display.println(name2);
  display.setCursor(20, 40);
  display.println(messwert);
  display.setCursor(70, 40);
  display.println(einheit);
  display.display();      // Display aktualisieren
}

/*
 * Funktion: DisplayIntro()
 * Spielt den Bootscreen auf dem Display ab und zeigt die IP Adresse an
 * String ip: IP Adresse des Chips
 * String hostname: Name des Gerätes -- im Browser dann errichbar unter hostname.local
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
  display.println(F("V0.2"));
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
 * Funktion: DisplayMesswerte(int bodenfeuchte, int helligkeit, int luftfeuchte, int lufttemperatur)
 * Stellt die Messwerte auf dem Display dar.
 * Wenn ein Messwert "-1" ist, wird die Anzeige übersprungen.
 * bodenfeuchte: Bodenfeuchte in %
 * helligkeit: Helligkeit in %
 * luftfeuchte: Luftfeuchte in %
 * lufttemperatur: Lufttemperatur in °C
 */
void DisplayMesswerte(int bodenfeuchte, String bodenfeuchteName, int helligkeit, String helligkeitName,
  float luftfeuchte, float lufttemperatur, int analog3, String analog3Name, int analog4, String analog4Name,
  int analog5, String analog5Name, int analog6, String analog6Name, int analog7, String analog7Name,
  int analog8, String analog8Name, int status) {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von DisplayMesswerte(")); Serial.print(helligkeit);
    Serial.print(F(", ")); Serial.print(luftfeuchte);
    Serial.print(F(", ")); Serial.print(lufttemperatur);
    Serial.print(F(", ")); Serial.print(status);
    Serial.println(F(")"));
  #endif

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  switch (status) {
    case 0:
      display.clearDisplay();
      display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
      display.display();
      break;
    case 1:
      display.clearDisplay();
      display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
      display.display();
      break;
    case 2:
      if (bodenfeuchte != -1) {
        std::pair<String, String> namen = NamenTeilen(bodenfeuchteName);
        MesswertAnzeigen(namen.first, namen.second, bodenfeuchte, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 3:
      if (helligkeit != -1) {
        std::pair<String, String> namen = NamenTeilen(helligkeitName);
        MesswertAnzeigen(namen.first, namen.second, helligkeit, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 4:
      if (lufttemperatur != -1) {
        MesswertAnzeigen("Lufttemp-", "eratur:", lufttemperatur, "\xf8 C");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 5:
      if (luftfeuchte != -1) {
        MesswertAnzeigen("Luft-", "feuchte:", luftfeuchte, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 6:
      if (analog3 != -1) {
        std::pair<String, String> namen = NamenTeilen(analog3Name);
        MesswertAnzeigen(namen.first, namen.second, analog3, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 7:
      if (analog4 != -1) {
        std::pair<String, String> namen = NamenTeilen(analog4Name);
        MesswertAnzeigen(namen.first, namen.second, analog4, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 8:
      if (analog5 != -1) {
        std::pair<String, String> namen = NamenTeilen(analog5Name);
        MesswertAnzeigen(namen.first, namen.second, analog5, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 9:
      if (analog6 != -1) {
        std::pair<String, String> namen = NamenTeilen(analog6Name);
        MesswertAnzeigen(namen.first, namen.second, analog6, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 10:
      if (analog7 != -1) {
        std::pair<String, String> namen = NamenTeilen(analog7Name);
        MesswertAnzeigen(namen.first, namen.second, analog7, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 11:
      if (analog8 != -1) {
        std::pair<String, String> namen = NamenTeilen(analog8Name);
        MesswertAnzeigen(namen.first, namen.second, analog8, "%");
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
  }
}
