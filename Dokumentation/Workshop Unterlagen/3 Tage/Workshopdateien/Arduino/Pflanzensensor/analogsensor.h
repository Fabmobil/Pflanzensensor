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

#include <Arduino.h>
#include <utility>

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
    int maximum);

#endif // ANALOGSENSOR_H
