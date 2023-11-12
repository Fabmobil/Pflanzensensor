/**
 * Multiplexer Modul
 * Diese Datei enthält den Code für das Multiplexer-Modul
 */

/*
 * Funktion: MultiplexerWechseln(int c, int b, int a)
 * Schaltet den Eingang des analog Multiplexers um:
 * 000 -> Eingang 0
 * 001 -> Eingang 1
 * 010 -> Eingang 2
 * 011 -> Eingang 3
 * 100 -> Eingang 4
 * 101 -> Eingang 5
 * 110 -> Eingang 6
 * 111 -> Eingang 7
 */
void MultiplexerWechseln(int c, int b, int a) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von MultiplexerWechseln(c, b, a)"));
    Serial.print(F("c: ")); Serial.print(c);
    Serial.print(F(" b: ")); Serial.print(b);
    Serial.print(F(" a: ")); Serial.println(a);
    Serial.println(F("#######################################"));
  #endif
  digitalWrite(pinMultiplexer1, a);
  digitalWrite(pinMultiplexer2, b);
  digitalWrite(pinMultiplexer3, c);
}
