/**
 * @file multiplexer.h
 * @brief Multiplexer Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Steuerung des analogen Multiplexers,
 * der es ermöglicht, mehrere Analogsensoren an einem einzelnen Analogeingang zu verwenden.
 */

#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

/**
 * @brief Schaltet den Eingang des analogen Multiplexers um
 *
 * @param a Wert für Digitaleingang A (0 oder 1)
 * @param b Wert für Digitaleingang B (0 oder 1)
 * @param c Wert für Digitaleingang C (0 oder 1)
 */
void MultiplexerWechseln(int a, int b, int c) {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von MultiplexerWechseln("));
    Serial.print(a); Serial.print(F(", "));
    Serial.print(b); Serial.print(F(", "));
    Serial.print(c); Serial.println(F(")"));
  #endif
  digitalWrite(multiplexerPinA, a); // Digitaleingang A setzen
  digitalWrite(multiplexerPinB, b); // Digitaleingang B setzen
  digitalWrite(multiplexerPinC, c); // Digitaleingang C setzen
  delay(100); // warten, bis der IC umgeschalten hat

}

#endif // MULTIPLEXER_H
