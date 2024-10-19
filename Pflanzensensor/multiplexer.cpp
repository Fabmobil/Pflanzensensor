#include "multiplexer.h"
#include <Arduino.h>
#include "einstellungen.h" // Für die Pin-Definitionen und andere globale Variablen
#include "logger.h" // Für die Logger-Funktionalität

void MultiplexerWechseln(int a, int b, int c) {
  logger.debug("Beginn von MultiplexerWechseln(" + String(a) + ", " + String(b) +", " + String(c) + ")");

  digitalWrite(multiplexerPinA, a); // Digitaleingang A setzen
  digitalWrite(multiplexerPinB, b); // Digitaleingang B setzen
  digitalWrite(multiplexerPinC, c); // Digitaleingang C setzen
  delay(100); // warten, bis der IC umgeschalten hat
}
