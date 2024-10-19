/**
 * @file wifi_seite_debug.h
 * @brief Debug-Informationsseite des Pflanzensensors
 * @author Tommy
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
 * @brief Generiert und sendet die Debug-Informationsseite
 *
 * Diese Funktion sammelt alle relevanten Debug-Informationen und
 * sendet sie als HTML-Seite an den Client.
 */
void WebseiteDebugAusgeben();

#endif // WIFI_SEITE_DEBUG_H
