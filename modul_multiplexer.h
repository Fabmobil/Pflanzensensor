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
    Serial.println(F("## Debug: Beginn von MultiplexerWechseln(a)"));
    Serial.print(F(" a: ")); Serial.println(a);
    Serial.println(F("#######################################"));
  #endif
  digitalWrite(pinMultiplexer, a);
}
