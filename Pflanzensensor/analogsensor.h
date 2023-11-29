std::pair<int, int> AnalogsensorMessen(
    int a,
    int b,
    int c,
    String sensorname,
    int minimum,
    int maximum) {
    // Ggfs. Multiplexer umstellen:
    #if MODUL_MULTIPLEXER
        MultiplexerWechseln(a, b, c); // Multiplexer auf Ausgang 0 stellen
    #endif
    // Helligkeit messen:
    int messwert = analogRead(pinAnalog); // Messwert einlesen
    // Messwert in Prozent umrechnen:
    int messwertProzent = map(messwert, minimum, maximum, 0, 100); // Skalierung auf maximal 0 bis 100
    Serial.print(sensorname); Serial.print(F(": ")); Serial.print(messwertProzent);
    Serial.print(F("%       (Messwert: ")); Serial.print(messwert);
    Serial.println(F(")"));
    return std::make_pair(messwert, messwertProzent);
}
