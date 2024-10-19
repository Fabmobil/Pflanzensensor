/**
 * @file dht.cpp
 * @brief DHT Luftfeuchte- und Lufttemperatursensor Modul Implementierung
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Implementierungen von Funktionen zum Auslesen des DHT Sensors für
 * Luftfeuchtigkeit und Lufttemperatur.
 */

#include "einstellungen.h" // Für dhtPin und dhtSensortyp
#include "dht.h"
#include "logger.h"

DHT_Unified dht(dhtPin, dhtSensortyp);

float MesseLuftfeuchtigkeit() {
  logger.debug("Beginn von MesseLuftfeuchtigkeit()");

  sensors_event_t ereignis; // Ereignis-Objekt für die Messung
  float luftfeuchtigkeit = -1; // Initialisierung der Luftfeuchtigkeitsvariable mit Fehlerwert

  dht.humidity().getEvent(&ereignis); // Durchführung der Luftfeuchtigkeitsmessung

  // Überprüfung, ob die Messung erfolgreich war
  if (isnan(ereignis.relative_humidity)) {
    logger.error("Luftfeuchtigkeitsmessung fehlgeschlagen!");
  }
  else {
    luftfeuchtigkeit = ereignis.relative_humidity; // Speichern des gemessenen Wertes
    logger.info("Gemessene Luftfeuchtigkeit: " + String(luftfeuchtigkeit) + "%");
  }

  return luftfeuchtigkeit; // Rückgabe des Messwertes oder des Fehlerwertes
}

float MesseLufttemperatur() {
  logger.debug("Beginn von messeLufttemperatur()");

  sensors_event_t ereignis; // Ereignis-Objekt für die Messung
  float lufttemperatur = -1; // Initialisierung der Lufttemperaturvariable mit Fehlerwert

  dht.temperature().getEvent(&ereignis); // Durchführung der Lufttemperaturmessung

  // Überprüfung, ob die Messung erfolgreich war
  if (isnan(ereignis.temperature)) {
    logger.error("Lufttemperaturmessung fehlgeschlagen!");
  }
  else {
    lufttemperatur = ereignis.temperature; // Speichern des gemessenen Wertes
    logger.info("Gemessene Lufttemperatur: " + String(lufttemperatur) + "°C");
  }

  return lufttemperatur; // Rückgabe des Messwertes oder des Fehlerwertes
}
