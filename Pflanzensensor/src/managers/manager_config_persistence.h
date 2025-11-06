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
   * @brief Save all Preferences to flash before filesystem update
   * @return True if save successful, false otherwise
   * @details Creates JSON with all settings and stores in firmware flash area
   */
  static bool savePreferencesToFlash();

  /**
   * @brief Restore all Preferences from flash after filesystem update  
   * @return True if restore successful, false otherwise
   * @details Reads JSON from flash and restores all settings to Preferences
   */
  static bool restorePreferencesFromFlash();

  /**
   * @brief Backup all Preferences to a JSON file before filesystem update
   * @return True if backup successful, false otherwise
   * @details Creates /prefs_backup.json with all settings (WiFi, sensors, display, etc.)
   * @deprecated Use savePreferencesToFlash() instead - this backs up to LittleFS which gets wiped
   */
  static bool backupPreferencesToFile();

  /**
   * @brief Restore all Preferences from backup JSON file after filesystem update
   * @return True if restore successful, false otherwise
   * @details Reads /prefs_backup.json and restores all settings to Preferences
   * @deprecated Use restorePreferencesFromFlash() instead
   */
  static bool restorePreferencesFromFile();

private:
  ConfigPersistence() = default;

  /**
   * @brief Load sensor error flags from JSON
   * @param sensorErrors JSON document containing sensor errors
   * @param config Configuration data to populate
   */
  static void loadSensorErrors(const ArduinoJson::JsonObject& sensorErrors, ConfigData& config);
};

#endif
