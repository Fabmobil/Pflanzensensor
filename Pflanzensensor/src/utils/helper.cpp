// helper.cpp
#include "utils/helper.h"

#include <LittleFS.h>

#include "../filesystem/config_fs.h"
#include "logger/logger.h"
#include "utils/critical_section.h"

// Forward declaration for the static helper in wifi.cpp
extern bool tryAllWiFiCredentials();

String Helper::getFormattedDate() {
  time_t now = getCurrentTime();
  if (now == 0) {
    return "???";
  }

  struct tm* timeinfo = localtime(&now);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%02d.%02d.%04d", timeinfo->tm_mday, timeinfo->tm_mon + 1,
           timeinfo->tm_year + 1900);
  return String(buffer);
}

String Helper::getFormattedTime(bool use24Hour) {
  time_t now = getCurrentTime();
  if (now == 0) {
    return "???";
  }

  struct tm* timeinfo = localtime(&now);
  char buffer[32];

  if (use24Hour) {
    snprintf(buffer, sizeof(buffer), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  } else {
    int hour12 = (timeinfo->tm_hour % 12) ? (timeinfo->tm_hour % 12) : 12;
    snprintf(buffer, sizeof(buffer), "%02d:%02d", hour12, timeinfo->tm_min);
  }
  return String(buffer);
}

time_t Helper::getCurrentTime() {
  if (!logger.isNTPInitialized()) {
    return 0;
  }
  return logger.getSynchronizedTime();
}

uint32_t Helper::getRebootCount() {
  CriticalSection cs;

  // Reboot count stored on MAIN_FS
  if (!MainFS.exists(REBOOT_COUNT_FILE)) {
    return 0;
  }

  File file = MainFS.open(REBOOT_COUNT_FILE, "r");
  if (!file) {
    return 0;
  }

  String countStr = file.readString();
  file.close();
  return countStr.toInt();
}

String Helper::getFormattedUptime() {
  unsigned long uptime = millis() / 1000;
  unsigned int days = uptime / 86400;
  unsigned int hours = (uptime % 86400) / 3600;
  unsigned int minutes = (uptime % 3600) / 60;

  String formatted;
  if (days > 0)
    formatted += String(days) + "d ";
  if (hours > 0)
    formatted += String(hours) + "h ";
  formatted += String(minutes) + "m";

  return formatted;
}

ResourceResult Helper::incrementRebootCount() {
  CriticalSection cs;

  // MAIN_FS is already mounted by DualFS init
  // Reboot count stored on MAIN_FS (not critical data)
  File file = MainFS.open(F("/reboot_count.txt"), "r");
  int count = 0;
  if (file) {
    count = file.parseInt();
    file.close();
  }
  count++;

  file = MainFS.open(F("/reboot_count.txt"), "w");
  if (!file) {
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR,
                                F("Fehler beim Öffnen der Neustartzähler-Datei zum Schreiben"));
  }

  file.println(count);
  file.close();
  logger.debug(F("Helper"), F("Neustartzähler erhöht auf: ") + String(count));
  return ResourceResult::success();
}

ResourceResult Helper::initializeUpgradeMode() {
  WiFi.mode(WIFI_STA);
  if (!tryAllWiFiCredentials()) {
    return ResourceResult::fail(
        ResourceError::WIFI_ERROR,
        F("Verbindung mit WLAN im Upgrade-Modus fehlgeschlagen (alle Credentials)"));
  }
  logger.info(F("Helper"), F("WLAN im Upgrade-Modus verbunden"));
  logger.info(F("Helper"), F("IP: ") + WiFi.localIP().toString());

  // Initialize time synchronization
  logger.info(F("Helper"), F("Initialisiere NTP im Minimalmodus..."));
  logger.initNTP();
  // Wait for time sync
  int retries = 0;
  while (retries < 10) {
    if (logger.getSynchronizedTime() > 24 * 3600) { // Time is after Jan 1, 1970
      break;
    }
    delay(1000);
    logger.updateNTP();
    retries++;
  }
  if (!WebManager::getInstance().beginUpdateMode()) {
    return ResourceResult::fail(ResourceError::OPERATION_FAILED,
                                F("Starten des WebManagers im Update-Modus fehlgeschlagen"));
  }
  return ResourceResult::success();
}

#if USE_DISPLAY
void Helper::displayWiFiConnectionAttempts(DisplayManager* displayManager,
                                           const String& attemptsInfo, bool isBootMode) {
  if (!displayManager || attemptsInfo.length() == 0) {
    return;
  }

  // Match the WiFi module's German phrasing for missing credentials
  if (attemptsInfo.indexOf("Keine WiFi-Zugangsdaten") >= 0 ||
      attemptsInfo.indexOf("Keine Credentials") >= 0) {
    displayManager->updateLogStatus(F("Keine WiFi-Zugangsdaten konfiguriert"), isBootMode);
  } else {
    // Split the long info into multiple lines for better readability
    int startPos = 0;
    int commaPos = attemptsInfo.indexOf(',', startPos);
    while (commaPos > 0) {
      String line = attemptsInfo.substring(startPos, commaPos);
      line.trim();
      if (line.length() > 0) {
        displayManager->updateLogStatus(line, isBootMode);
      }
      startPos = commaPos + 1;
      commaPos = attemptsInfo.indexOf(',', startPos);
    }
    // Show the last part
    String lastLine = attemptsInfo.substring(startPos);
    lastLine.trim();
    if (lastLine.length() > 0) {
      displayManager->updateLogStatus(lastLine, isBootMode);
    }
  }
}
#endif // USE_DISPLAY
