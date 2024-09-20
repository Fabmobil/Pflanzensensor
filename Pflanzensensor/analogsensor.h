/**
 * @file analogsensor.h
 * @brief Funktionen zur Verarbeitung von Analogsensoren
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zum Auslesen und Verarbeiten von Analogsensoren,
 * einschließlich der Umrechnung von Rohwerten in Prozentwerte.
 */

#ifndef ANALOGSENSOR_H
#define ANALOGSENSOR_H

/**
 * @brief Misst den Wert eines Analogsensors und berechnet den Prozentwert
 *
 * Diese Funktion schaltet den Multiplexer (falls vorhanden), liest den Analogwert ein
 * und berechnet den entsprechenden Prozentwert basierend auf den gegebenen Minimum- und Maximumwerten.
 *
 * @param a Multiplexer-Einstellung A
 * @param b Multiplexer-Einstellung B
 * @param c Multiplexer-Einstellung C
 * @param sensorname Name des Sensors (für Debugging)
 * @param minimum Minimaler Rohwert des Sensors
 * @param maximum Maximaler Rohwert des Sensors
 * @return std::pair<int, int> Paar von (Rohwert, Prozentwert)
 */
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

#endif // ANALOGSENSOR_H
