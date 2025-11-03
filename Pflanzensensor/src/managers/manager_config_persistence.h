/**
 * @file manager_config_persistence.h
 * @brief Configuration file persistence layer
 */

#ifndef MANAGER_CONFIG_PERSISTENCE_H
#define MANAGER_CONFIG_PERSISTENCE_H

#include <ArduinoJson.h>

#include "../utils/result_types.h"
#include "configs/config.h"
#include "manager_config_types.h"

class ConfigPersistence {
public:
  using PersistenceResult = TypedResult<ConfigError, void>;

  /**
   * @brief Load configuration from file
   * @param config Configuration data structure to populate
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult loadFromFile(ConfigData& config);

  /**
   * @brief Save configuration to file (minimal, no String or logger)
   * @param config Configuration data to save
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult saveToFileMinimal(const ConfigData& config);

  /**
   * @brief Reset configuration to default values
   * @param config Configuration data structure to reset
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult resetToDefaults(ConfigData& config);

  /**
   * @brief Check if configuration file exists
   * @return True if config file exists, false otherwise
   */
  static bool configFileExists();

  /**
   * @brief Get configuration file size
   * @return Size of config file in bytes, 0 if file doesn't exist
   */
  static size_t getConfigFileSize();

  /**
   * @brief Write update flags to a simple text file (not JSON config)
   * @param fs Filesystem update pending
   * @param fw Firmware update pending
   */
  static void writeUpdateFlagsToFile(bool fs, bool fw);

  /**
   * @brief Read update flags from a simple text file (not JSON config)
   * @param fs Filesystem update pending (output)
   * @param fw Firmware update pending (output)
   */
  static void readUpdateFlagsFromFile(bool& fs, bool& fw);

private:
  ConfigPersistence() = default;

  /**
   * @brief Load configuration from JSON file (for migration)
   * @param config Configuration data structure to populate
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult loadFromJSON(ConfigData& config);

  /**
   * @brief Save configuration to Preferences
   * @param config Configuration data to save
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult saveToPreferences(const ConfigData& config);

  /**
   * @brief Load sensor error flags from JSON
   * @param sensorErrors JSON document containing sensor errors
   * @param config Configuration data to populate
   */
  static void loadSensorErrors(const ArduinoJson::JsonObject& sensorErrors, ConfigData& config);
};

// Function to apply sensor settings directly from JSON
void applySensorSettingsFromJson(const String& sensorId, const JsonObject& sensorConfig);

#endif
