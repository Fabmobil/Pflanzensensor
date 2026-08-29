/**
 * @file manager_sensor_persistence.h
 * @brief SensorPersistence-Ersatz für native Unit-Tests
 *
 * Der Messzyklus schreibt Messwerte über eine Write-Behind-Queue, die
 * letztlich in LittleFS landet - dafür gibt es hier kein Gerät. Die drei
 * benutzten Methoden werden zu Nichts; PendingUpdates zu prüfen ist Sache
 * einer eigenen Testsuite für manager_sensor_persistence.cpp, nicht dieser.
 */

#ifndef NATIVE_TEST_MANAGER_SENSOR_PERSISTENCE_H
#define NATIVE_TEST_MANAGER_SENSOR_PERSISTENCE_H

#include <Arduino.h>
#include <cstddef>

class SensorPersistence {
public:
  static void enqueueAbsoluteMinMax(const String&, size_t, float, float) {}
  static void enqueueLastValue(const String&, size_t, float, float) {}
  static void flushPendingUpdatesForSensor(const String&) {}
};

#endif // NATIVE_TEST_MANAGER_SENSOR_PERSISTENCE_H
