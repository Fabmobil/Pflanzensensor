/**
 * @file manager_config.h
 * @brief ConfigMgr-Ersatz für native Unit-Tests
 *
 * Aus dem echten ConfigMgr benutzen die getesteten Einheiten nur
 * isDebugMeasurementCycle(). Der Rest hinge an Preferences und LittleFS.
 */

#ifndef NATIVE_TEST_MANAGER_CONFIG_H
#define NATIVE_TEST_MANAGER_CONFIG_H

#include <Arduino.h>

class ConfigManagerStub {
public:
  bool isDebugMeasurementCycle() const { return m_debugMeasurementCycle; }
  void setDebugMeasurementCycle(bool value) { m_debugMeasurementCycle = value; }

private:
  bool m_debugMeasurementCycle{false};
};

inline ConfigManagerStub ConfigMgr;

#endif // NATIVE_TEST_MANAGER_CONFIG_H
