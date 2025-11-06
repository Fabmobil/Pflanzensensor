/**
 * @file flash_persistence.h
 * @brief Flash-based configuration persistence for OTA filesystem updates
 * 
 * This module stores configuration data in unused firmware flash space,
 * allowing settings to survive filesystem OTA updates on ESP8266.
 * 
 * Strategy:
 * - Store config JSON in sketch partition beyond compiled code
 * - Calculate safe offset dynamically using ESP.getSketchSize()
 * - Data survives FS updates (firmware partition untouched)
 * - Read back after FS update and restore to Preferences
 */

#ifndef FLASH_PERSISTENCE_H
#define FLASH_PERSISTENCE_H

#include <Arduino.h>
#include "../utils/result_types.h"

/**
 * @class FlashPersistence
 * @brief Manages configuration storage in firmware flash area
 * 
 * Uses unused space in the sketch partition to store configuration
 * as JSON during filesystem OTA updates. The data survives because
 * filesystem OTA only updates the LittleFS partition, not the sketch.
 */
class FlashPersistence {
public:
  /**
   * @brief Save configuration JSON to flash
   * @param jsonData Configuration data as JSON string
   * @return ResourceResult indicating success or failure
   */
  static ResourceResult saveToFlash(const String& jsonData);

  /**
   * @brief Load configuration JSON from flash
   * @param jsonData Output parameter for loaded JSON string
   * @return ResourceResult indicating success or failure
   */
  static ResourceResult loadFromFlash(String& jsonData);

  /**
   * @brief Clear configuration data from flash
   * @return ResourceResult indicating success or failure
   */
  static ResourceResult clearFlash();

  /**
   * @brief Check if valid configuration exists in flash
   * @return true if valid config found, false otherwise
   */
  static bool hasValidConfig();

  /**
   * @brief Get the safe flash offset for storing data
   * @return Flash address offset for safe storage
   */
  static uint32_t getSafeOffset();

  /**
   * @brief Get maximum size available for config storage
   * @return Maximum bytes available
   */
  static uint32_t getMaxSize();

private:
  FlashPersistence() = default;

  // Magic number to identify valid stored data
  static constexpr uint32_t MAGIC_NUMBER = 0x50464C54; // "PFLT" (Pflanzensensor)
  
  // Version number for compatibility checking
  static constexpr uint8_t VERSION = 1;

  // Safety margin beyond sketch size (in sectors, 1 sector = 4KB)
  static constexpr uint32_t SAFETY_MARGIN_SECTORS = 10;

  // Maximum size for config storage (64KB should be plenty)
  static constexpr uint32_t MAX_CONFIG_SIZE = 64 * 1024;

  // Flash sector size on ESP8266
  static constexpr uint32_t FLASH_SECTOR_SIZE = 4096;

  /**
   * @brief Calculate CRC32 checksum for data integrity
   * @param data Data to checksum
   * @param length Length of data
   * @return CRC32 checksum
   */
  static uint32_t calculateCRC32(const uint8_t* data, size_t length);

  /**
   * @brief Verify stored data integrity
   * @param offset Flash offset to check
   * @param size Expected data size
   * @param storedCRC Stored CRC value
   * @return true if data is valid
   */
  static bool verifyIntegrity(uint32_t offset, uint32_t size, uint32_t storedCRC);
};

#endif // FLASH_PERSISTENCE_H
