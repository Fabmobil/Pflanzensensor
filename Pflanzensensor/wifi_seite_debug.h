/**
 * @file wifi_seite_debug.h
 * @brief Debug-Informationsseite des Pflanzensensors
 * @author Tommy, Claude
 * @date 2023-09-20
 *
 * Diese Datei enthält Funktionen zur Generierung der Debug-Informationsseite
 * des Pflanzensensors.
 */

#ifndef WIFI_SEITE_DEBUG_H
#define WIFI_SEITE_DEBUG_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer Webserver;

/**
 * @brief Sendet einen HTML-Abschnitt mit Debug-Informationen
 *
 * Diese Funktion erstellt einen HTML-Abschnitt mit einem Titel und Inhalt
 * und sendet ihn an den Webserver.
 *
 * @param titel Der Titel des Abschnitts
 * @param inhalt Der Inhalt des Abschnitts als HTML-String
 */
void sendeDebugAbschnitt(const __FlashStringHelper* titel, const String& inhalt);

/**
 * @brief Generiert Debug-Informationen für einen Analogsensor
 *
 * Diese Funktion erstellt einen HTML-String mit Debug-Informationen
 * für einen Analogsensor.
 *
 * @param name Name des Sensors
 * @param webhook Webhook-Aktivierungsstatus
 * @param messwertProzent Messwert in Prozent
 * @param messwert Roher Messwert
 * @param minimum Minimaler Sensorwert
 * @param maximum Maximaler Sensorwert
 * @return String HTML-formatierte Debug-Informationen
 */
String generiereAnalogSensorInfo(const String& name, bool webhook, int messwertProzent, int messwert, int minimum, int maximum);

/**
 * @brief Generiert und sendet die Debug-Informationsseite
 *
 * Diese Funktion sammelt alle relevanten Debug-Informationen und
 * sendet sie als HTML-Seite an den Client. Sie erstellt Abschnitte für
 * allgemeine Informationen, aktive Module und deaktivierte Module.
 */
void WebseiteDebugAusgeben();

#endif // WIFI_SEITE_DEBUG_H
