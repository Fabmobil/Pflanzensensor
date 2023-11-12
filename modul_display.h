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

/**
 * Funktion: DisplayIntro()
 * Spielt den Bootscreen auf dem Display ab und zeigt die IP Adresse an
 * String ip: IP Adresse des Chips
 */
void DisplayIntro(String ip) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von DisplayIntro()"));
    Serial.println(F("#######################################"));
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
  display.println(F("V0.1"));
  display.display();      // Display aktualisieren
  delay(2000);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2); 
  display.println(F("IP Adresse"));
  display.setTextSize(1); 
  display.setCursor(0, 40);
  display.println(ip);
  display.display();
  delay(5000);
  
}

/**
 * Funktion: DisplayMesswerte(int bodenfeuchte, int lichtstaerke, int luftfeuchte, int lufttemperatur)
 * Stellt die Messwerte auf dem Display dar.
 * Wenn ein Messwert "-1" ist, wird die Anzeige übersprungen.
 * bodenfeuchte: Bodenfeuchte in %
 * lichtstaerke: Lichtstaerke in %
 * luftfeuchte: Luftfeuchte in %
 * lufttemperatur: Lufttemperatur in °C
 */
void DisplayMesswerte(int bodenfeuchte, int lichtstaerke, int luftfeuchte, int lufttemperatur) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von DisplayMesswerte(bodenfeuchte, lichtstaerke, luftfeuchte, lufttemperatur)"));
    Serial.print(F("Bodenfeuchte: ")); Serial.println(bodenfeuchte);
    Serial.print(F("Lichtstärke: ")); Serial.println(lichtstaerke);
    Serial.print(F("Luftfeuchte: ")); Serial.println(luftfeuchte);
    Serial.print(F("Lufttemperatur: ")); Serial.println(lufttemperatur);
    Serial.println(F("#######################################"));
  #endif
if (bodenfeuchte != -1) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Boden-"));
    display.setCursor(10, 20);
    display.println(F("feuchte:"));
    display.setCursor(0, 40);
    display.println(bodenfeuchte);
    display.display();      // Display aktualisieren
    delay(displayAnzeigedauer);
  }
  if (lichtstaerke != -1) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Licht-"));
    display.setCursor(10, 20);
    display.println(F("staerke:"));
    display.setCursor(0, 40);
    display.println(lichtstaerke);
    display.setCursor(50, 40);
    display.println("%");
    display.display();      // Display aktualisieren
    delay(displayAnzeigedauer);
  }
  if (lufttemperatur != -1) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Lufttemp-"));
    display.setCursor(10, 20);
    display.println(F("eratur:"));
    display.setCursor(0, 40);
    display.println(lufttemperatur);
    display.setCursor(50, 40);
    display.println("\xf8");
    display.setCursor(60, 40);
    display.println("C");
    display.display();      // Display aktualisieren
    delay(displayAnzeigedauer);
  }
  if (luftfeuchte != -1) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Luft-"));
    display.setCursor(10, 20);
    display.println(F("feuchte:"));
    display.setCursor(0, 40);
    display.println(luftfeuchte);
    display.setCursor(50, 40);
    display.println("%");
    display.display();      // Display aktualisieren
    delay(displayAnzeigedauer);
  }
}
