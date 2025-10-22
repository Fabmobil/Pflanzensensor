/**
 * @file manager_config_debug.h
 * @brief Debug configuration manager
 */

#ifndef MANAGER_CONFIG_DEBUG_H
#define MANAGER_CONFIG_DEBUG_H

#include "../utils/result_types.h"
#include "manager_config_types.h"

class ConfigNotifier; // Forward declaration

class DebugConfig {
public:
  using DebugResult = TypedResult<ConfigError, void>;

  /**
   * @brief Constructor
   * @param notifier Reference to the configuration notifier
   */
  explicit DebugConfig(ConfigNotifier& notifier);

  // Getters
  /**
   * @brief Check if RAM debugging is enabled
   * @return True if RAM debugging is enabled, false otherwise
   */
  bool isRAMDebugEnabled() const { return m_debugRAM; }

  /**
   * @brief Check if measurement cycle debugging is enabled
   * @return True if measurement cycle debugging is enabled, false otherwise
   */
  bool isMeasurementCycleDebugEnabled() const { return m_debugMeasurementCycle; }

  /**
   * @brief Check if sensor debugging is enabled
   * @return True if sensor debugging is enabled, false otherwise
   */
  bool isSensorDebugEnabled() const { return m_debugSensor; }

  /**
   * @brief Check if display debugging is enabled
   * @return True if display debugging is enabled, false otherwise
   */
  bool isDisplayDebugEnabled() const { return m_debugDisplay; }

  /**
   * @brief Check if WebSocket debugging is enabled
   * @return True if WebSocket debugging is enabled, false otherwise
   */
  bool isWebSocketDebugEnabled() const { return m_debugWebSocket; }

  // Setters
  /**
   * @brief Set the RAM debugging status
   * @param enabled True to enable RAM debugging, false to disable
   * @return Result of the set operation
   */
  DebugResult setRAMDebug(bool enabled);

  /**
   * @brief Set the measurement cycle debugging status
   * @param enabled True to enable measurement cycle debugging, false to disable
   * @return Result of the set operation
   */
  DebugResult setMeasurementCycleDebug(bool enabled);

  /**
   * @brief Set the sensor debugging status
   * @param enabled True to enable sensor debugging, false to disable
   * @return Result of the set operation
   */
  DebugResult setSensorDebug(bool enabled);

  /**
   * @brief Set the display debugging status
   * @param enabled True to enable display debugging, false to disable
   * @return Result of the set operation
   */
  DebugResult setDisplayDebug(bool enabled);

  /**
   * @brief Set the WebSocket debugging status
   * @param enabled True to enable WebSocket debugging, false to disable
   * @return Result of the set operation
   */
  DebugResult setWebSocketDebug(bool enabled);

  // Load/Save
  /**
   * @brief Load debug settings from configuration data
   * @param data Configuration data to load from
   */
  void loadFromConfigData(const ConfigData& data);

  /**
   * @brief Save debug settings to configuration data
   * @param data Configuration data to save to
   */
  void saveToConfigData(ConfigData& data) const;

private:
  ConfigNotifier& m_notifier;

  bool m_debugRAM = false;
  bool m_debugMeasurementCycle = false;
  bool m_debugSensor = false;
  bool m_debugDisplay = false;
  bool m_debugWebSocket = false;
};

#endif
