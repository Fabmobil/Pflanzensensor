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

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(displayBreite, displayHoehe, &Wire, displayReset); // Initialisierung des Displays

#include "modul_display_bilder.h" // Bilder fürs Display
/**
 * Funktion: DisplayIntro()
 * Spielt den Bootscreen auf dem Display ab und zeigt die IP Adresse an
 * String ip: IP Adresse des Chips
 * String hostname: Name des Gerätes -- im Browser dann errichbar unter hostname.local
 */
void DisplayIntro(String ip, String hostname) {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von DisplayIntro()"));
  #endif
  display.clearDisplay();

  display.setTextSize(2); // Doppelt großer Text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("FABMOBIL"));
  display.display();      // Display aktualisieren
  delay(100);

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

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(F("IP Adresse"));
  display.setTextSize(1);
  display.setCursor(0, 17);
  display.println(ip);
  display.setCursor(0, 35);
  display.setTextSize(2);
  display.println(F("Hostname"));
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print(hostname);
  display.println(".local");
  display.display();
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
void DisplayMesswerte(int bodenfeuchte, int helligkeit, float luftfeuchte, float lufttemperatur, int status) {
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
      if (bodenfeuchte != -1) {
        display.setCursor(0, 0);
        display.println(F("Boden-"));
        display.setCursor(10, 20);
        display.println(F("feuchte:"));
        display.setCursor(20, 40);
        display.println(bodenfeuchte);
        display.setCursor(70, 40);
        display.println("%");
        display.display();      // Display aktualisieren
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 1:
      if (helligkeit != -1) {
        display.setCursor(0, 0);
        display.println(F("Hellig-"));
        display.setCursor(10, 20);
        display.println(F("keit:"));
        display.setCursor(20, 40);
        display.println(helligkeit);
        display.setCursor(70, 40);
        display.println("%");
        display.display();      // Display aktualisieren
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 2:
      if (lufttemperatur != -1) {
        display.setCursor(0, 0);
        display.println(F("Lufttemp-"));
        display.setCursor(10, 20);
        display.println(F("eratur:"));
        display.setCursor(20, 40);
        display.println(lufttemperatur);
        display.setCursor(85, 40);
        display.println("\xf8");
        display.setCursor(95, 40);
        display.println("C");
        display.display();      // Display aktualisieren
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 3:
      if (luftfeuchte != -1) {
        display.setCursor(0, 0);
        display.println(F("Luft-"));
        display.setCursor(10, 20);
        display.println(F("feuchte:"));
        display.setCursor(20, 40);
        display.println(luftfeuchte);
        display.setCursor(85, 40);
        display.println("%");
        display.display();      // Display aktualisieren
      } else {
        display.clearDisplay();
        display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
        display.display();
      }
      break;
    case 4:
      display.clearDisplay();
      display.drawBitmap(0, 0, bildFabmobil, displayBreite, displayHoehe, WHITE);
      display.display();
      break;
    case 5:
      display.clearDisplay();
      display.drawBitmap(0, 0, bildBlume, displayBreite, displayHoehe, WHITE);
      display.display();
      break;
  }
}