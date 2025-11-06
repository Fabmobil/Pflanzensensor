/**
 * @file config_fs.h
 * @brief Dual LittleFS partition management for OTA-safe configuration
 * 
 * This module manages two separate LittleFS partitions:
 * 1. CONFIG partition (64KB) - Mounted as global LittleFS for Preferences
 * 2. MAIN_FS partition (~844KB) - For web assets, logs, etc.
 * 
 * The CONFIG partition is mounted first as the global LittleFS object,
 * ensuring the Preferences library stores data there. The MAIN_FS partition
 * is mounted separately and can be safely updated via OTA without losing settings.
 * 
 * Flash Layout:
 * - Sketch (Firmware)  : 0x40200000 - 0x40389000 (~1575KB)
 * - OTA (Firmware)     : 0x40389000 - 0x40512000 (~1575KB)
 * - CONFIG Partition   : 0x40510000 - 0x40520000 (64KB)     - Preferences
 * - MAIN_FS Partition  : 0x40520000 - 0x405F3000 (~844KB)   - Web assets
 * - EEPROM             : 0x405F3000 - 0x405F4000 (4KB)
 * - RF Calibration     : 0x405FB000 - 0x405FC000 (4KB)
 * - WiFi Config        : 0x405FD000 - 0x40600000 (12KB)
 */

#ifndef CONFIG_FS_H
#define CONFIG_FS_H

#include <Arduino.h>
#include <LittleFS.h>
#include "../utils/result_types.h"

/**
 * @class DualFS
 * @brief Manager for dual LittleFS partitions (CONFIG + MAIN_FS)
 * 
 * This class manages two separate LittleFS partitions:
 * - CONFIG partition: Used by Preferences library (mounted as global LittleFS)
 * - MAIN_FS partition: Used for web assets and can be updated via OTA
 * 
 * The CONFIG partition MUST be mounted first (as LittleFS) before any
 * Preferences usage to ensure settings are stored there.
 */
class DualFS {
public:
  /**
   * @brief Get the singleton instance
   */
  static DualFS& getInstance();

  /**
   * @brief Initialize both filesystems (CONFIG first, then MAIN_FS)
   * @return ResourceResult indicating success or failure
   */
  ResourceResult init();

  /**
   * @brief Mount the CONFIG partition (as global LittleFS for Preferences)
   * @return ResourceResult indicating success or failure
   */
  ResourceResult mountConfigFS();

  /**
   * @brief Mount the MAIN_FS partition (for web assets)
   * @return ResourceResult indicating success or failure
   */
  ResourceResult mountMainFS();

  /**
   * @brief Format the CONFIG partition
   * @return ResourceResult indicating success or failure
   */
  ResourceResult formatConfigFS();

  /**
   * @brief Format the MAIN_FS partition
   * @return ResourceResult indicating success or failure
   */
  ResourceResult formatMainFS();

  /**
   * @brief Check if CONFIG partition is mounted
   * @return true if mounted, false otherwise
   */
  bool isConfigMounted() const;

  /**
   * @brief Check if MAIN_FS partition is mounted
   * @return true if mounted, false otherwise
   */
  bool isMainMounted() const;

  /**
   * @brief Get the MAIN_FS filesystem object
   * @return Reference to the main FS object
   */
  fs::FS& getMainFS();

  /**
   * @brief Get CONFIG filesystem information
   * @param info Reference to FSInfo structure to fill
   * @return true on success, false on failure
   */
  bool getConfigInfo(FSInfo& info);

  /**
   * @brief Get MAIN_FS filesystem information
   * @param info Reference to FSInfo structure to fill
   * @return true on success, false on failure
   */
  bool getMainInfo(FSInfo& info);

private:
  DualFS();
  ~DualFS() = default;
  
  // Prevent copying
  DualFS(const DualFS&) = delete;
  DualFS& operator=(const DualFS&) = delete;

  // Main filesystem for web assets (separate instance)
  fs::LittleFSFS _mainFS;
  
  bool _configMounted;
  bool _mainMounted;

  // Partition locations (from linker script)
  static constexpr uint32_t CONFIG_START = 0x40510000;
  static constexpr uint32_t CONFIG_END = 0x40520000;
  static constexpr uint32_t CONFIG_SIZE = CONFIG_END - CONFIG_START; // 64KB
  
  static constexpr uint32_t MAIN_FS_START = 0x40520000;
  static constexpr uint32_t MAIN_FS_END = 0x405F3000;
  static constexpr uint32_t MAIN_FS_SIZE = MAIN_FS_END - MAIN_FS_START; // ~844KB
};

// Global accessor for dual filesystem manager
#define DualFSInstance DualFS::getInstance()

// Global accessor for main filesystem (for web assets, logs, etc.)
// Note: LittleFS global object is used for CONFIG (Preferences)
#define MainFS DualFSInstance.getMainFS()

#endif // CONFIG_FS_H
