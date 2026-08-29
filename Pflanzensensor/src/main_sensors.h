/**
 * @file main_sensors.h
 * @brief Sensor manager initialization module header
 */

#ifndef MAIN_SENSORS_H
#define MAIN_SENSORS_H

#include "utils/result_types.h"
#include <Arduino.h>

// Forward declarations for globals defined in main.cpp
class SensorManager;
class DisplayManager;

extern std::unique_ptr<SensorManager> sensorManager;
extern std::unique_ptr<DisplayManager> displayManager;

ResourceResult initializeSensors();
void showSensorStatus();

#endif // MAIN_SENSORS_H
