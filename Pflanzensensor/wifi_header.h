
/**
 * @file wifi_header.h
 * @brief HTML-Header für Webseiten des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält den HTML-Header, der auf allen Webseiten
 * des Pflanzensensors verwendet wird.
 */

#ifndef WIFI_HEADER_H
#define WIFI_HEADER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

// Deklarieren Sie die Header-Teile als externe PROGMEM-Strings
extern const char PROGMEM HTML_HEAD_START[];
extern const char PROGMEM HTML_HEAD_END[];
extern const char PROGMEM HTML_BODY_START[];
extern const char PROGMEM HTML_HEADER_CONTENT[];

// Funktion zum Senden des Headers
void sendeHtmlHeader(ESP8266WebServer& server, bool mitRefresh = false);

#endif // WIFI_HEADER_H

