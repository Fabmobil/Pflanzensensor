/**
 * @file main_web.h
 * @brief Web server initialization module header
 */

#ifndef MAIN_WEB_H
#define MAIN_WEB_H

#include "utils/result_types.h"
#include <Arduino.h>

// Forward declarations for globals defined in main.cpp
class SensorManager;
class DisplayManager;
class WebManager;

extern std::unique_ptr<SensorManager> sensorManager;
extern std::unique_ptr<DisplayManager> displayManager;
extern WebManager& webManager;

ResourceResult initializeWebServer();
void showWebServerStatus();

#endif // MAIN_WEB_H
