/**
 * @file analogsensor.cpp
 * @brief Implementierung der Funktionen zur Verarbeitung von Analogsensoren
 * @author Tommy
 * @date 2023-09-20
 */

#include "analogsensor.h"
#include "logger.h"
#include "einstellungen.h"

#if MODUL_MULTIPLEXER
#include "multiplexer.h"
#endif

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

    // Analogwert messen:
    int messwert = analogRead(pinAnalog); // Messwert einlesen

    // Messwert in Prozent umrechnen:
    int messwertProzent = map(messwert, minimum, maximum, 0, 100); // Skalierung auf maximal 0 bis 100

    // Logging des Messergebnisses
    logger.info(sensorname + ": " + String(messwertProzent) + "%       (Messwert: " + String(messwert) + ")");

    return std::make_pair(messwert, messwertProzent);
}
