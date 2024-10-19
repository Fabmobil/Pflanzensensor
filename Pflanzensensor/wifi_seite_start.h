/**
 * @file wifi_seite_start.h
 * @brief Startseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält Funktionsdeklarationen zur Generierung der Startseite
 * des Pflanzensensors mit aktuellen Sensordaten.
 */

#ifndef WIFI_SEITE_START_H
#define WIFI_SEITE_START_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "einstellungen.h"

extern ESP8266WebServer Webserver;

/**
 * @brief Sendet Sensordaten an den Webserver
 *
 * @param sensorName Name des Sensors
 * @param sensorFarbe Farbcodierung des Sensorwerts (z.B. "rot", "gelb", "gruen")
 * @param messwert Aktueller Messwert des Sensors
 * @param einheit Einheit des Messwerts
 * @param alarm Gibt an, ob ein Alarm für diesen Sensor aktiv ist
 * @param webhook Gibt an, ob Webhook-Benachrichtigungen für diesen Sensor aktiviert sind
 */
void sendeSensorDaten(const __FlashStringHelper* sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit, bool alarm, bool webhook);

/**
 * @brief Sendet Daten eines Analogsensors an den Webserver
 *
 * @param sensorNummer Nummer des Analogsensors
 * @param sensorName Name des Sensors
 * @param sensorFarbe Farbcodierung des Sensorwerts
 * @param messwert Aktueller Messwert des Sensors
 * @param einheit Einheit des Messwerts
 * @param alarm Gibt an, ob ein Alarm für diesen Sensor aktiv ist
 * @param webhook Gibt an, ob Webhook-Benachrichtigungen für diesen Sensor aktiviert sind
 */
void sendeAnalogsensorDaten(int sensorNummer, const String& sensorName, const String& sensorFarbe, int messwert, const __FlashStringHelper* einheit, bool alarm, bool webhook);

/**
 * @brief Generiert und sendet die Startseite mit allen aktuellen Sensordaten
 */
void WebseiteStartAusgeben();

#endif // WIFI_SEITE_START_H
