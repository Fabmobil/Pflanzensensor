/**
 * Multiplexer Modul
 * Diese Datei enthält den Code für das Multiplexer-Modul
 */

/*
 * Funktion: MultiplexerWechseln(int a)
 * Schaltet den Eingang des analog Multiplexers um:
 * 0 -> Eingang 0
 * 1 -> Eingang 1
 */
void MultiplexerWechseln(int a) {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von MultiplexerWechseln("));
    Serial.print(a);
    Serial.println(")");
  #endif
  digitalWrite(pinMultiplexer, a);
}
