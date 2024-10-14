/**
 * @file dht.h
 * @brief DHT Luftfeuchte- und Lufttemperatursensor Modul
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zum Auslesen des DHT Sensors für
 * Luftfeuchtigkeit und Lufttemperatur.
 */

#ifndef DHTS_H
#define DHTS_H

#include <Adafruit_Sensor.h> // Adafruit Sensor Library
#include <DHT.h> // DHT Sensor Library
#include <DHT_U.h> // DHT Unified Sensor Library

#include "logger.h"

DHT_Unified dht(dhtPin, dhtSensortyp); // DHT Sensor initialisieren

/**
 * @brief Misst die Luftfeuchtigkeit mit dem DHT-Sensor
 *
 * Diese Funktion führt eine Messung der Luftfeuchtigkeit mit dem DHT-Sensor durch
 * und gibt das Ergebnis als Fließkommazahl zurück.
 *
 * @return float Gemessene Luftfeuchtigkeit in Prozent, oder -1 bei Messfehler
 */
float DhtMessenLuftfeuchte() { // Luftfeuchte messen
  logger.debug("## Debug: Beginn von DhtMessenLuftfeuchte()");
  logger.debug("DHT PIN: " + String(dhtPin));
  logger.debug("DHT Sensortyp: "+ String(dhtSensortyp));

  sensors_event_t event; // Event für die Messung
  float luftfeuchte = -1; // Variable für die Luftfeuchte
  dht.humidity().getEvent(&event); // Messung der Luftfeuchte
  // überprüfen ob die Messung erfolgreich war:
  if (isnan(event.relative_humidity)) { // überprüfen ob die Messung erfolgreich war
    logger.error("Luftfeuchtemessung nicht erfolgreich! :-(");
  }
  else {
    logger.info("Luftfeuchte: " + String(event.relative_humidity) + "%");
    luftfeuchte = event.relative_humidity; // Luftfeuchte in Prozent
  }
  return luftfeuchte; // Luftfeuchte zurückgeben
}



/**
 * @brief Misst die Lufttemperatur mit dem DHT-Sensor
 *
 * Diese Funktion führt eine Messung der Lufttemperatur mit dem DHT-Sensor durch
 * und gibt das Ergebnis als Fließkommazahl zurück.
 *
 * @return float Gemessene Lufttemperatur in °C, oder -1 bei Messfehler
 */
float DhtMessenLufttemperatur() { // Lufttemperatur messen
  logger.debug("## Debug: Beginn von DhtMessenLufttemperatur()");
  logger.debug("DHT PIN: " + String(dhtPin));
  logger.debug("DHT Sensortyp: " + String(dhtSensortyp));

  sensors_event_t event; // Event für die Messung
  float lufttemperatur = -1; // Variable für die Lufttemperatur
  dht.temperature().getEvent(&event); // Messung der Lufttemperatur
  if (isnan(event.temperature)) { // überprüfen ob die Messung erfolgreich war
    logger.error("Temperaturmessung nicht erfolgreich! :-(");
  }
  else {
    logger.info("Lufttemperatur: " + String(event.temperature) + "°C");
  }
  return lufttemperatur; // Lufttemperatur zurückgeben
}

#endif // DHTS_H
