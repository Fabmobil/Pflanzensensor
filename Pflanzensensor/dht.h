/**
 * @file dht.h
 * @brief DHT Luftfeuchte- und Lufttemperatursensor Modul
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zum Auslesen des DHT Sensors für
 * Luftfeuchtigkeit und Lufttemperatur.
 */

#ifndef DHT_H
#define DHT_H

#include <Adafruit_Sensor.h> // Adafruit Sensor Library
#include <DHT.h> // DHT Sensor Library
#include <DHT_U.h> // DHT Unified Sensor Library

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
  #if MODUL_DEBUG // Debuginformation
    Serial.println(F("## Debug: Beginn von DhtMessenLuftfeuchte()"));
    Serial.print(F("DHT PIN: ")); Serial.println(dhtPin);
    Serial.print(F("DHT Sensortyp: ")); Serial.println(dhtSensortyp);
  #endif
  sensors_event_t event; // Event für die Messung
  float luftfeuchte = -1; // Variable für die Luftfeuchte
  dht.humidity().getEvent(&event); // Messung der Luftfeuchte
  // überprüfen ob die Messung erfolgreich war:
  if (isnan(event.relative_humidity)) { // überprüfen ob die Messung erfolgreich war
    Serial.println(F("Luftfeuchtemessung nicht erfolgreich! :-("));
  }
  else {
    Serial.print(F("Luftfeuchte: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
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
#if MODUL_DEBUG
  Serial.println(F("## Debug: Beginn von DhtMessenLufttemperatur()"));
  Serial.print(F("DHT PIN: ")); Serial.println(dhtPin);
  Serial.print(F("DHT Sensortyp: ")); Serial.println(dhtSensortyp);
#endif
  sensors_event_t event; // Event für die Messung
  float lufttemperatur = -1; // Variable für die Lufttemperatur
  dht.temperature().getEvent(&event); // Messung der Lufttemperatur
  if (isnan(event.temperature)) { // überprüfen ob die Messung erfolgreich war
    Serial.println(F("Temperaturmessung nicht erfolgreich! :-("));
  }
  else {
    Serial.print(F("Lufttemperatur: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    lufttemperatur = event.temperature; // Lufttemperatur in °C
  }
  return lufttemperatur; // Lufttemperatur zurückgeben
}

#endif // DHT_H
