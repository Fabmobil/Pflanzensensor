/**
 * @file ledampel.h
 * @brief LED-Ampel Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Steuerung der LED-Ampel,
 * die visuelle Rückmeldungen über den Zustand der Pflanze gibt.
 */

#ifndef LEDAMPEL_H
#define LEDAMPEL_H
#include "logger.h"

/**
 * @brief Lässt die LED-Ampel in einer bestimmten Farbe blinken
 *
 * @param farbe String; "rot", "gruen" oder "gelb"
 * @param anzahl Integer; Anzahl der Blinkvorgänge
 * @param dauer Integer; Dauer eines Blinkvorgangs in Millisekunden
 */
void LedampelBlinken(String farbe, int anzahl, int dauer) {
  logger.debug("# Beginn von LedampelBlinken()");
  logger.debug("# Farbe: " + String(farbe) + ", Anzahl: "+ String(anzahl) + ", Dauer: " + String(dauer));

  char PIN_LED;
  digitalWrite(ampelPinRot, LOW);
  digitalWrite(ampelPinGelb, LOW);
  digitalWrite(ampelPinGruen, LOW);
  if (farbe == "rot") { // wenn die Farbe rot ist, wird der Pin für die rote LED gesetzt
    PIN_LED = ampelPinRot;
  }
  if (farbe =="gelb") { // wenn die Farbe gelb ist, wird der Pin für die gelbe LED gesetzt
    PIN_LED = ampelPinGelb;
  }
  if (farbe == "gruen") { // wenn die Farbe grün ist, wird der Pin für die grüne LED gesetzt
    PIN_LED = ampelPinGruen;
  }
  for (int i=0;i<anzahl;i++){ // Schleife für die Anzahl der Blinkvorgänge
    digitalWrite(PIN_LED, HIGH); // LED an
    delay(dauer); // warten
    digitalWrite(PIN_LED, LOW); // LED aus
    delay(dauer); // warten
  }
}


/**
 * @brief Lässt die LED-Ampel in einer bestimmten Farbe leuchten
 *
 * @param farbe String; "rot", "gruen" oder "gelb"
 * @param dauer Integer; Dauer der Farbanzeige in Millisekunden. Bei -1 bleibt die LED an.
 */
void LedampelAnzeigen(String farbe, int dauer) {
  logger.debug("# Beginn von LedampelAnzeigen(" + String(farbe) + ", " + String(dauer) + ")");

  digitalWrite(ampelPinRot, LOW); // alle LEDs aus
  digitalWrite(ampelPinGelb, LOW);
  digitalWrite(ampelPinGruen, LOW);
  if (farbe == "rot") { // wenn die Farbe rot ist, wird der Pin für die rote LED gesetzt
    digitalWrite(ampelPinRot, HIGH);
    if (dauer != -1) { // wenn die Dauer nicht -1 ist, wird die LED nach der Dauer wieder ausgeschaltet
      delay(dauer);
      digitalWrite(ampelPinRot, LOW);
    }
  }
  if (farbe == "gelb") { // wenn die Farbe gelb ist, wird der Pin für die gelbe LED gesetzt
    digitalWrite(ampelPinGelb, HIGH);
    if (dauer != -1) { // wenn die Dauer nicht -1 ist, wird die LED nach der Dauer wieder ausgeschaltet
      delay(dauer);
      digitalWrite(ampelPinGelb, LOW);
    }
  }
  if (farbe == "gruen") { // wenn die Farbe grün ist, wird der Pin für die grüne LED gesetzt
    digitalWrite(ampelPinGruen, HIGH);
    if (dauer != -1) { // wenn die Dauer nicht -1 ist, wird die LED nach der Dauer wieder ausgeschaltet
      delay(dauer);
      digitalWrite(ampelPinGruen, LOW);
    }
  }
}

/**
 * @brief Schaltet alle LEDs der Ampel aus
 */
void LedampelAus() {
  digitalWrite(ampelPinGruen, LOW);
  digitalWrite(ampelPinGelb, LOW);
  digitalWrite(ampelPinRot, LOW);
}

#endif // LEDAMPEL_H
