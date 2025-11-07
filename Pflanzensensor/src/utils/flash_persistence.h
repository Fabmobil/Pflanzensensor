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

private:
  FlashPersistence() = default;

  // Magic number to identify our data
  static constexpr uint32_t FP_MAGIC_NUMBER = 0x50464C54; // "PFLT"
  static constexpr uint8_t FP_VERSION = 2;                // Version 2 = simple text format

  // Flash layout constants
  static constexpr uint32_t FP_SAFETY_MARGIN_SECTORS = 10;
  static constexpr uint32_t FP_MAX_CONFIG_SIZE = 64 * 1024;
  static constexpr uint32_t FP_FLASH_SECTOR_SIZE = 4096;

  static uint32_t getSafeOffset();
  static uint32_t calculateCRC32(const uint8_t* data, size_t length);
};

#endif // FLASH_PERSISTENCE_H
