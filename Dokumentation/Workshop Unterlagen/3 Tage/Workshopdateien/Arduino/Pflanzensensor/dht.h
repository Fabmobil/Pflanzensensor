/**
 * @file dht.h
 * @brief DHT Luftfeuchte- und Lufttemperatursensor Modul Header
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieser Header enthält Deklarationen für Funktionen zum Auslesen des DHT Sensors für
 * Luftfeuchtigkeit und Lufttemperatur.
 */

#ifndef DHTS_H
#define DHTS_H

#include "einstellungen.h"
#include <Adafruit_Sensor.h> // Adafruit Sensor Bibliothek
#include <DHT.h> // DHT Sensor Bibliothek
#include <DHT_U.h> // DHT Unified Sensor Bibliothek

extern DHT_Unified dht; // Extern-Deklaration für die globale DHT-Variable

/**
 * @brief Misst die Luftfeuchtigkeit mit dem DHT-Sensor
 *
 * @return float Gemessene Luftfeuchtigkeit in Prozent, oder -1 bei Messfehler
 */
float MesseLuftfeuchtigkeit();

/**
 * @brief Misst die Lufttemperatur mit dem DHT-Sensor
 *
 * @return float Gemessene Lufttemperatur in °C, oder -1 bei Messfehler
 */
float MesseLufttemperatur();

#endif // DHTS_H
