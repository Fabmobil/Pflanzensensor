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
   * @brief Load configuration from Preferences
   * @param config Configuration data structure to populate
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult load(ConfigData& config);

  /**
   * @brief Save configuration to Preferences
   * @param config Configuration data to save
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult save(const ConfigData& config);

  /**
   * @brief Reset configuration to default values
   * @param config Configuration data structure to reset
   * @return PersistenceResult indicating success or failure
   */
  static PersistenceResult resetToDefaults(ConfigData& config);

  /**
   * @brief Check if configuration exists in Preferences
   * @return True if config exists, false otherwise
   */
  static bool configExists();

  /**
   * @brief Get estimated configuration size in Preferences
   * @return Estimated size in bytes
   */
  static size_t getConfigSize();

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

  /**
   * @brief Backup all config data (Preferences + JSON sensor files) to /prefs_backup.json
   * @return True if backup successful, false otherwise
   * @details Creates /prefs_backup.json containing:
   *          - Global settings (WiFi, Display, Debug, NTP, InfluxDB) from Preferences
   *          - Sensor measurements from /config/sensor_*.json files
   *          Used for Config Download in WebUI
   */
  static bool backupPreferencesToFile();

  /**
   * @brief Restore config data from parsed JSON document
   * @param doc Parsed JSON document containing config backup
   * @return True if restore successful, false otherwise
   * @details Helper function used by restorePreferencesFromFile for Config Upload
   *          Restores both global Preferences and sensor JSON files
   */
  static bool restorePreferencesFromJson(const DynamicJsonDocument& doc);

private:
  ConfigPersistence() = default;
};

#endif
