#include "multiplexer.h"
#include <Arduino.h>
#include "einstellungen.h" // Für die Pin-Definitionen und andere globale Variablen
#include "logger.h" // Für die Logger-Funktionalität

void MultiplexerWechseln(int a, int b, int c) {
  logger.debug(F("Beginn von MultiplexerWechseln(") + String(a) + F(", ") + String(b) + F(", ") + String(c) + F(")"));

  digitalWrite(multiplexerPinA, a); // Digitaleingang A setzen
  digitalWrite(multiplexerPinB, b); // Digitaleingang B setzen
  digitalWrite(multiplexerPinC, c); // Digitaleingang C setzen
  delay(100); // warten, bis der IC umgeschalten hat
}
