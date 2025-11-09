/**
 * @file flash_persistence.h
 * @brief Text-based flash persistence (no JSON, minimal heap usage)
 */

#ifndef FLASH_PERSISTENCE_H
#define FLASH_PERSISTENCE_H

#include "../utils/result_types.h"
#include <Arduino.h>
#include <Preferences.h>

/**
 * @class FlashPersistence
 * @brief Stores Preferences as simple key=value text format in flash
 *
 * Format: Each line is "namespace:key=value"
 * No JSON parsing needed - just String.indexOf() and substring()
 * Minimal heap allocations during restore
 */
class FlashPersistence {
public:
  /**
   * @brief Save all Preferences to flash as simple text
   */
  static ResourceResult saveToFlash();

  /**
   * @brief Restore all Preferences from flash text
   */
  static ResourceResult restoreFromFlash();

  /**
   * @brief Clear flash storage
   */
  static ResourceResult clearFlash();

  /**
   * @brief Check if valid backup exists
   */
  static bool hasValidConfig();

  /**
   * @brief Save all Preferences AND config JSON files to flash
   * @return ResourceResult indicating success or failure
   */
  static ResourceResult saveAllToFlash();

  /**
   * @brief Restore all Preferences AND config JSON files from flash
   * @return ResourceResult indicating success or failure
   */
  static ResourceResult restoreAllFromFlash();

private:
  FlashPersistence() = default;

  // Magic number to identify our data
  static constexpr uint32_t FP_MAGIC_NUMBER = 0x50464C54; // "PFLT"
  static constexpr uint8_t FP_VERSION = 4;                // Version 4 = text format + config files

  // Flash layout constants
  static constexpr uint32_t FP_SAFETY_MARGIN_SECTORS = 10;
  static constexpr uint32_t FP_MAX_CONFIG_SIZE = 64 * 1024;
  static constexpr uint32_t FP_FLASH_SECTOR_SIZE = 4096;

  // Separate storage areas to avoid heap exhaustion
  static constexpr uint32_t FP_PREFS_MAX_SIZE = 8 * 1024; // 8KB for Preferences
  static constexpr uint32_t FP_JSON_MAX_SIZE = 32 * 1024; // 32KB for JSON configs

  static uint32_t getSafeOffset();
  static uint32_t getJsonStorageOffset(); // Second area for JSON files
  static uint32_t calculateCRC32(const uint8_t* data, size_t length);

  // Helper methods for JSON storage
  static ResourceResult saveJsonToFlash();
  static ResourceResult restoreJsonFromFlash();
};

#endif // FLASH_PERSISTENCE_H
