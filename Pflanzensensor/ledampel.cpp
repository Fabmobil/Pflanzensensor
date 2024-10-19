#include "ledampel.h"
#include "logger.h"
#include "einstellungen.h"

void LedampelBlinken(String farbe, int anzahl, int dauer) {
  logger.debug("Beginn von LedampelBlinken(" + String(farbe) + ", " + String(anzahl) + ", " + String(dauer));

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

void LedampelAnzeigen(String farbe, int dauer) {
  logger.debug("Beginn von LedampelAnzeigen(" + String(farbe) + ", " + String(dauer) + ")");

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

void LedampelAus() {
  digitalWrite(ampelPinGruen, LOW);
  digitalWrite(ampelPinGelb, LOW);
  digitalWrite(ampelPinRot, LOW);
}
