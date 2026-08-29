/**
 * @file platform_compat.h
 * @brief Platform compatibility definitions for ESP8266 and ESP32
 */

#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#ifdef ESP32
#include <WebServer.h>
#include <WiFi.h>
using ESPWebServer = WebServer;

// On ESP32, F() cannot be concatenated with + operator directly
// Use FS() for Flash Strings that need to be concatenated
#define FS(string_literal) String(F(string_literal))
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
using ESPWebServer = ESP8266WebServer;

// On ESP8266, F() can be concatenated directly, but FS() works too
#define FS(string_literal) F(string_literal)
#endif

#endif // PLATFORM_COMPAT_H
