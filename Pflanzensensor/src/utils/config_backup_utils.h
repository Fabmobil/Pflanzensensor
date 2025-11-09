/**
 * @file config_backup_utils.h
 * @brief Utility functions for backing up and restoring config files during OTA updates
 */

#ifndef CONFIG_BACKUP_UTILS_H
#define CONFIG_BACKUP_UTILS_H

#include "../logger/logger.h"
#include "flash_persistence.h"
#include <Arduino.h>

namespace ConfigBackupUtils {

/**
 * @brief Backup all Preferences AND JSON config files to firmware flash
 * @return true if backup was successful, false otherwise
 *
 * This function is called before a filesystem OTA update to preserve
 * both Preferences and sensor configurations. Everything is stored in
 * firmware flash so it survives both LittleFS and NVS erasure.
 */
inline bool backupConfigFiles() {
  logger.info(F("ConfigBackup"), F("Sichere Preferences + Config-Dateien in Firmware-Flash..."));

  auto result = FlashPersistence::saveAllToFlash();
  if (!result.isSuccess()) {
    logger.error(F("ConfigBackup"), F("Flash-Backup fehlgeschlagen: ") + result.getMessage());
    return false;
  }

  logger.info(F("ConfigBackup"), F("Erfolgreich in Firmware-Flash gesichert"));
  return true;
}

/**
 * @brief Restore all Preferences AND JSON config files from firmware flash
 * @return true if restore was successful, false otherwise
 *
 * This function is called after a filesystem OTA update to restore
 * previously backed up Preferences and sensor configurations from firmware flash.
 */
inline bool restoreConfigFiles() {
  logger.info(F("ConfigRestore"),
              F("Stelle Preferences + Config-Dateien aus Firmware-Flash wieder her..."));

  auto result = FlashPersistence::restoreAllFromFlash();
  if (!result.isSuccess()) {
    logger.error(F("ConfigRestore"), F("Flash-Restore fehlgeschlagen: ") + result.getMessage());
    return false;
  }

  logger.info(F("ConfigRestore"), F("Erfolgreich aus Firmware-Flash wiederhergestellt"));
  return true;
}

} // namespace ConfigBackupUtils

#endif // CONFIG_BACKUP_UTILS_H
