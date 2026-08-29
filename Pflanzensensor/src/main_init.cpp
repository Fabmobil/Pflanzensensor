/**
 * @file main_init.cpp
 * @brief System initialization and recovery module
 * @details Handles filesystem initialization, flash recovery, and reboot tracking
 */

#include <Arduino.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "utils/config_backup_utils.h"
#include "utils/critical_section.h"
#include "utils/flash_persistence.h"
#include "utils/helper.h"

// Forward declarations
extern Logger logger;
#if USE_DISPLAY
extern std::unique_ptr<DisplayManager> displayManager;
#endif

/**
 * @brief Detect and handle boot loops
 * @details Tracks rapid reboots using a counter file. If the device reboots
 *          3+ times within BOOT_LOOP_WINDOW_MS (30s), it's considered a boot
 *          loop. In that case: clear firmware upgrade flag and reset the counter.
 *          The counter is reset to 0 after BOOT_LOOP_WINDOW_MS of stable uptime.
 */
static void checkBootLoop() {
  constexpr const char* BOOT_LOOP_FILE = "/.boot_loop";
  constexpr int BOOT_LOOP_THRESHOLD = 3;
  constexpr unsigned long BOOT_LOOP_WINDOW_MS = 30000; // 30 seconds

  // Read current count and timestamp
  int count = 0;
  unsigned long lastBootMs = 0;
  if (LittleFS.exists(BOOT_LOOP_FILE)) {
    File f = LittleFS.open(BOOT_LOOP_FILE, "r");
    if (f) {
      count = f.parseInt();
      lastBootMs = f.parseInt();
      f.close();
    }
  }

  unsigned long now = millis();

  // If last boot was recent, increment; otherwise reset
  if (count > 0 && (now - lastBootMs) < BOOT_LOOP_WINDOW_MS) {
    count++;
  } else {
    count = 1;
  }

  // Write updated counter
  File f = LittleFS.open(BOOT_LOOP_FILE, "w");
  if (f) {
    f.print(count);
    f.print(' ');
    f.print(now);
    f.close();
  }

  if (count >= BOOT_LOOP_THRESHOLD) {
    Serial.print(F("BOOT LOOP DETECTED ("));
    Serial.print(count);
    Serial.println(F(" reboots). Clearing firmware upgrade flag and resetting counter."));

    // Clear the boot loop counter
    LittleFS.remove(BOOT_LOOP_FILE);

    // Check if stuck in firmware upgrade mode and clear it
    // (Preferences API not yet initialized here, so we use the config manager later)
    // Write a flag file that main.cpp will pick up after full init
    File flagFile = LittleFS.open("/.clear_upgrade_flag", "w");
    if (flagFile) {
      flagFile.print("1");
      flagFile.close();
    }

    Serial.println(F("Boot loop recovery flag written. Continuing normal boot."));
  }
}

/**
 * @brief Initialize filesystem and check for recovery needs
 * @return true if initialization successful, false otherwise
 * @details
 * 1. Mount LittleFS
 * 2. Detect boot loops and handle recovery
 * 3. Check for flash restore flag
 * 4. Execute recovery if needed
 * 5. Return success/failure status
 */
bool initializeSystem() {
  // **ULTRA-CRITICAL: Check for flash restore BEFORE ANYTHING ELSE**
  // We need to restore with the absolute cleanest heap possible
  // Don't even initialize logger yet - heap fragmentation is critical
  {
    if (!LittleFS.begin()) {
      Serial.println(F("FATAL: Dateisystem-Mount fehlgeschlagen"));
      return false;
    }
  }

  // Boot loop detection - must run before config is loaded
  checkBootLoop();

  // Check for recovery flag
  if (LittleFS.exists("/.restore_from_flash")) {
    logger.info(F("main_init"),
                F("Wiederherstellungs-Flag gefunden - stelle Konfiguration wieder her"));

    // Remove flag FIRST to prevent infinite loops
    LittleFS.remove("/.restore_from_flash");

    if (FlashPersistence::hasValidConfig()) {
      Serial.print(F("Freier Heap vor Wiederherstellung: "));
      Serial.println(ESP.getFreeHeap());

      // Call restore directly - minimal allocations
      auto result = FlashPersistence::restoreFromFlash();
      if (result.isSuccess()) {
        logger.info(F("main_init"), F("Preferences erfolgreich wiederhergestellt"));

        // Restore JSON config files from /backup/ to /config/
        logger.info(F("main_init"), F("Stelle Config-Dateien wieder her..."));
        if (ConfigBackupUtils::restoreConfigFiles()) {
          logger.info(F("main_init"), F("Config-Dateien erfolgreich wiederhergestellt"));
        } else {
          logger.warning(F("main_init"), F("Keine Config-Dateien zum Wiederherstellen gefunden"));
        }

        logger.info(F("main_init"), F("Wiederherstellung abgeschlossen - starte neu..."));
        delay(1000);
        ESP.restart(); // Reboot with restored config
        return false;  // Should never reach here
      } else {
        logger.error(F("main_init"),
                     String(F("Wiederherstellung fehlgeschlagen: ")) + result.getMessage());
      }
    } else {
      logger.warning(F("main_init"), F("Kein gültiges Flash-Backup gefunden"));
    }
  }

  return true;
}

/**
 * @brief Show boot progress on display
 * @param message Message to display
 * @param clearClearPrevious Whether to clear previous message
 */
void showBootProgress(const String& message, bool clearPrevious = false) {
#if USE_DISPLAY
  if (displayManager) {
    displayManager->updateLogStatus(message, clearPrevious);
  }
#endif
}
