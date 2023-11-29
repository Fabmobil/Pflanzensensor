/**
 * Multiplexer Modul
 * Diese Datei enthält den Code für das Multiplexer-Modul
 * Der eingesetzte ESP8266 hat nur einen analogen Eingang. Es wird deshalb ein 4051 Chip eingesetzt. Das ist ein
 * analog Multiplexer. Er hat 8 Analogeingänge, 1 Analogausgang und 3 Digitaleingänge.
 * Je nach Bitmuster auf den Digitaleingängen wird immer genau ein Analogeingang mit dem Ausgang verbunden.
 * Wir nutzen hier nur 2 der 3 Digitaleingänge, da wir nicht genügend Ausgänge auf dem ESP8266 übrig haben.
 * Der dritte Eingang ist mit Masse verbunden und damit immer 0. Damit können bis zu 4 Analogsensoren ausgelesen
 * werden. Der Code nutzt davon gerade nur 2.
 */

/*
 * Funktion: MultiplexerWechseln(int a, int b)
 * Schaltet den Eingang des analog Multiplexers um:
 * a=0, b=0, c=0 -> Eingang 0, derzeit ungenutzt
 * a=1, b=0, c=0 -> Eingang 1, derzeit ungenutzt
 * a=0, b=1, c=0 -> Eingang 2, derzeit ungenutzt
 * a=1, b=1, c=0 -> Eingang 3, derzeit ungenutzt
 * a=0, b=0, c=1 -> Eingang 4, derzeit ungenutzt
 * a=1, b=0, c=1 -> Eingang 5, derzeit ungenutzt
 * a=0, b=1, c=1 -> Eingang 6, Bodenfeuchtigkeitsmesser
 * a=1, b=1, c=1 -> Eingang 7, Helligkeitssensor
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
