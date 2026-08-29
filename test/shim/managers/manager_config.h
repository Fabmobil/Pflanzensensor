/**
 * @file manager_config.h
 * @brief ConfigMgr-Ersatz für native Unit-Tests
 *
 * Aus dem echten ConfigMgr benutzt die getestete Logik nur
 * isDebugMeasurementCycle() und isDebugSensor(). Der Rest hinge an
 * Preferences und LittleFS.
 *
 * Thresholds ist eigentlich in managers/manager_config_types.h definiert.
 * Diese Datei ist selbst schlank (nur Arduino.h, <map>, config.h,
 * result_types.h), wird hier aber nicht eingebunden, um nicht versehentlich
 * weitere Abhängigkeiten der echten Konfigurationsverwaltung hereinzuziehen -
 * der Typ wird deshalb direkt nachgebaut.
 */

#ifndef NATIVE_TEST_MANAGER_CONFIG_H
#define NATIVE_TEST_MANAGER_CONFIG_H

#include <Arduino.h>

struct Thresholds {
  float yellowLow;
  float greenLow;
  float greenHigh;
  float yellowHigh;
};

class ConfigManagerStub {
public:
  bool isDebugMeasurementCycle() const { return m_debugMeasurementCycle; }
  void setDebugMeasurementCycle(bool value) { m_debugMeasurementCycle = value; }

  bool isDebugSensor() const { return m_debugSensor; }
  void setDebugSensor(bool value) { m_debugSensor = value; }

private:
  bool m_debugMeasurementCycle{false};
  bool m_debugSensor{false};
};

inline ConfigManagerStub ConfigMgr;

#endif // NATIVE_TEST_MANAGER_CONFIG_H
