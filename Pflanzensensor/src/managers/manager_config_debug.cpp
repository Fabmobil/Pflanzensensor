/**
 * @file manager_config_debug.cpp
 * @brief Implementation of DebugConfig class
 */

#include "manager_config_debug.h"

#include "manager_config_notifier.h"
#include "../logger/logger.h"

DebugConfig::DebugConfig(ConfigNotifier& notifier) : m_notifier(notifier) {}

DebugConfig::DebugResult DebugConfig::setRAMDebug(bool enabled) {
  if (m_debugRAM != enabled) {
    m_debugRAM = enabled;
    m_notifier.notifyChange("debug_ram", enabled ? "true" : "false", false);
    logger.info(F("DebugCfg"), String(F("RAM-Debug gesetzt: ")) + (enabled ? F("true") : F("false")));
  }
  return DebugResult::success();
}

DebugConfig::DebugResult DebugConfig::setMeasurementCycleDebug(bool enabled) {
  if (m_debugMeasurementCycle != enabled) {
    m_debugMeasurementCycle = enabled;
    m_notifier.notifyChange("debug_measurement_cycle",
                            enabled ? "true" : "false", false);
    logger.info(F("DebugCfg"), String(F("Messzyklus-Debug gesetzt: ")) + (enabled ? F("true") : F("false")));
  }
  return DebugResult::success();
}

DebugConfig::DebugResult DebugConfig::setSensorDebug(bool enabled) {
  if (m_debugSensor != enabled) {
    m_debugSensor = enabled;
    // When sensor debug flag changes we want to propagate changes to sensors
    logger.info(F("DebugCfg"), String(F("Sensor-Debug gesetzt: ")) + (enabled ? F("true") : F("false")));
    m_notifier.notifyChange("debug_sensor", enabled ? "true" : "false", true);
  }
  return DebugResult::success();
}

DebugConfig::DebugResult DebugConfig::setDisplayDebug(bool enabled) {
  if (m_debugDisplay != enabled) {
    m_debugDisplay = enabled;
    m_notifier.notifyChange("debug_display", enabled ? "true" : "false", false);
    logger.info(F("DebugCfg"), String(F("Display-Debug gesetzt: ")) + (enabled ? F("true") : F("false")));
  }
  return DebugResult::success();
}

DebugConfig::DebugResult DebugConfig::setWebSocketDebug(bool enabled) {
  if (m_debugWebSocket != enabled) {
    m_debugWebSocket = enabled;
    m_notifier.notifyChange("debug_websocket", enabled ? "true" : "false",
                            false);
    logger.info(F("DebugCfg"), String(F("WebSocket-Debug gesetzt: ")) + (enabled ? F("true") : F("false")));
  }
  return DebugResult::success();
}

void DebugConfig::loadFromConfigData(const ConfigData& data) {
  m_debugRAM = data.debugRAM;
  m_debugMeasurementCycle = data.debugMeasurementCycle;
  m_debugSensor = data.debugSensor;
  m_debugDisplay = data.debugDisplay;
  m_debugWebSocket = data.debugWebSocket;
}

void DebugConfig::saveToConfigData(ConfigData& data) const {
  data.debugRAM = m_debugRAM;
  data.debugMeasurementCycle = m_debugMeasurementCycle;
  data.debugSensor = m_debugSensor;
  data.debugDisplay = m_debugDisplay;
  data.debugWebSocket = m_debugWebSocket;
}
