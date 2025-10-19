/**
 * @file Logger.cpp
 * @brief Implementation of the Logger class with web support, indented console
 * output, and file logging
 */

#include "logger.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>  // For timezone support
#include <umm_malloc/umm_malloc.h>

#include "configs/config.h"
#include "managers/manager_config.h"
#include "utils/critical_section.h"
#if USE_WEBSOCKET
#include "web/handler/log_handler.h"
#endif

const char Logger::MSG_MEMORY_STATS[] PROGMEM =
  "Speicher [%s] Heap:%u/%u Block:%u Stack:%u/%u Frag:%u%%";
const char Logger::MSG_FREE_HEAP[] PROGMEM = "- Freier Heap: %u Bytes";
const char Logger::MSG_MAX_FREE_BLOCK[] PROGMEM = "- Größter freier Block: %u Bytes";
const char Logger::MSG_FRAGMENTATION[] PROGMEM = "- Fragmentierung: %u%%";
const char Logger::MSG_FREE_CONT_STACK[] PROGMEM =
  "- Freier Cont-Stack: %u Bytes";
const char Logger::MSG_FREE_STACK[] PROGMEM = "- Freier Stack: %u Bytes";
const char Logger::MSG_BYTES[] PROGMEM = " Bytes";
const char Logger::MSG_INITIALIZING[] PROGMEM = "Initialisiere ";
const char Logger::MSG_SUCCESS[] PROGMEM = "Erfolg: ";
const char Logger::MSG_ERROR[] PROGMEM = "#E#";
const char Logger::MSG_WARNING[] PROGMEM = "!W!";
const char Logger::MSG_DEBUG[] PROGMEM = ".D.";
const char Logger::MSG_INFO[] PROGMEM = ":I:";
const char Logger::MSG_BEFORE[] PROGMEM = "vorher";
const char Logger::MSG_AFTER[] PROGMEM = "nachher";
const char Logger::MSG_MEMORY_CHANGES[] PROGMEM =
  "Speicheränderungen [%s] Heap:%+d Block:%+d Stack:%+d Frag:%+d%%";

// Determine default log level from compile-time macro LOG_LEVEL (e.g., "INFO")
static LogLevel getDefaultLogLevelFromConfig() {
#ifdef LOG_LEVEL
  if (strcmp(LOG_LEVEL, "DEBUG") == 0) return LogLevel::DEBUG;
  if (strcmp(LOG_LEVEL, "INFO") == 0) return LogLevel::INFO;
  if (strcmp(LOG_LEVEL, "WARNING") == 0) return LogLevel::WARNING;
  if (strcmp(LOG_LEVEL, "ERROR") == 0) return LogLevel::ERROR;
#endif
  return LogLevel::INFO;  // Fallback if not defined or invalid
}

Logger logger(getDefaultLogLevelFromConfig(), true, FILE_LOGGING_ENABLED);

// Forward declaration of log callback type
using LogCallback = std::function<void(LogLevel, const String&)>;
static LogCallback s_logCallback = nullptr;

Logger::Logger(LogLevel logLevel, bool useSerial, bool fileLoggingEnabled)
    : m_logLevel(logLevel),
      m_useSerial(useSerial),
      m_useColors(false),
      m_timeClient(nullptr),
      m_ntpInitialized(false),
      m_fileLoggingEnabled(false) {  // Start false, will be enabled after setup
  if (m_useSerial) {
    Serial.begin(115200);
  }

  // Initialize file logging if requested
  if (fileLoggingEnabled) {
    // Mount filesystem first with critical section
    {
      CriticalSection cs;
      if (!LittleFS.begin()) {
        if (m_useSerial) {
          Serial.println(F("Dateisystem für Logging konnte nicht eingehängt werden"));
        }
        return;
      }

      // Create log file if it doesn't exist
      if (!LittleFS.exists(m_logFileName)) {
        File file = LittleFS.open(m_logFileName, "w");
        if (file) {
          file.println(F("Logdatei erstellt"));
          file.close();
        } else if (m_useSerial) {
          Serial.println(F("Initiale Logdatei konnte nicht erstellt werden"));
          return;
        }
      }
    }

    m_fileLoggingEnabled = true;
    if (m_useSerial) {
  Serial.println(F("Dateilogs erfolgreich initialisiert"));
    }
  }
}

void Logger::debug(const String& module, const String& message) {
  log(LogLevel::DEBUG, module, message);
}

void Logger::info(const String& module, const String& message) {
  log(LogLevel::INFO, module, message);
}

void Logger::warning(const String& module, const String& message) {
  log(LogLevel::WARNING, module, message);
}

void Logger::error(const String& module, const String& message) {
  log(LogLevel::ERROR, module, message);
}

void Logger::log(LogLevel level, const String& module, const String& message) {
  if (level < m_logLevel) {
    return;
  }

  // Safety check: replace empty or undefined messages
  String safeMessage = message;
  if (!safeMessage.length()) {
    safeMessage = F("LEERE LOG-NACHRICHT");
  }

  String timestamp = getFormattedTimestamp();
  String prefix;
  switch (level) {
    case LogLevel::DEBUG:
      prefix = readProgmemString(MSG_DEBUG);
      break;
    case LogLevel::INFO:
      prefix = readProgmemString(MSG_INFO);
      break;
    case LogLevel::WARNING:
      prefix = readProgmemString(MSG_WARNING);
      break;
    case LogLevel::ERROR:
      prefix = readProgmemString(MSG_ERROR);
      break;
  }

  char formattedMessage[128];
  snprintf(formattedMessage, sizeof(formattedMessage), "%s [%s] %s",
           prefix.c_str(), module.c_str(), safeMessage.c_str());

  if (m_useSerial) {
    String serialMessage;
    if (m_useColors) {
      // Add simple color codes for better compatibility
      switch (level) {
        case LogLevel::DEBUG:
          serialMessage = "\x1b[90m" + timestamp + " " + formattedMessage +
                          "\x1b[0m";  // Grey
          break;
        case LogLevel::INFO:
          serialMessage = "\x1b[32m" + timestamp + " " + formattedMessage +
                          "\x1b[0m";  // Green
          break;
        case LogLevel::WARNING:
          serialMessage = "\x1b[33m" + timestamp + " " + formattedMessage +
                          "\x1b[0m";  // Orange
          break;
        case LogLevel::ERROR:
          serialMessage = "\x1b[31m" + timestamp + " " + formattedMessage +
                          "\x1b[0m";  // Red
          break;
      }
    } else {
      serialMessage = timestamp + " " + formattedMessage;
    }
    Serial.println(serialMessage);
  }

  if (m_fileLoggingEnabled) {
    String plainMessage = timestamp + " " + formattedMessage;
    writeToFile(plainMessage);
  }

  // Call the log callback if set
  if (s_logCallback) {
    s_logCallback(level, String(formattedMessage));
  }
}

void Logger::setLogLevel(LogLevel level) {
  m_logLevel = level;
  debug("Logger", String(F("Log-Level gesetzt auf: ")) + logLevelToString(level));
}

LogLevel Logger::getLogLevel() const { return m_logLevel; }

MemoryStats Logger::getMemoryStats() {
  MemoryStats stats;
  stats.freeHeap = ESP.getFreeHeap();
  stats.maxFreeBlock = ESP.getMaxFreeBlockSize();
  stats.fragmentation = static_cast<uint8_t>(ESP.getHeapFragmentation());
  stats.freeContStack = ESP.getFreeContStack();
  stats.freeStack = ESP.getFreeHeap() - ESP.getMaxFreeBlockSize();

// Get total heap size - ESP8266 doesn't have getHeapSize()
#ifdef ESP32
  stats.totalHeap = ESP.getHeapSize();
#else
  stats.totalHeap = 81920;  // ESP8266 typically has 80KB heap
#endif

  stats.totalStack =
      ESP.getFreeContStack() + (ESP.getFreeHeap() - ESP.getMaxFreeBlockSize());

  // Update peak values
  updatePeakStats(stats);

  return stats;
}

void Logger::updatePeakStats(const MemoryStats& stats) {
  m_peakStats.minFreeHeap = min(m_peakStats.minFreeHeap, stats.freeHeap);
  m_peakStats.minFreeBlock = min(m_peakStats.minFreeBlock, stats.maxFreeBlock);
  m_peakStats.maxFragmentation =
      max(m_peakStats.maxFragmentation, stats.fragmentation);
}

void Logger::logMemoryStats(const String& location) {
  if (!ConfigMgr.isDebugRAM()) return;
  MemoryStats stats = getMemoryStats();

  char buffer[128];
  snprintf_P(buffer, sizeof(buffer), MSG_MEMORY_STATS, location.c_str(),
             stats.freeHeap, stats.totalHeap, stats.maxFreeBlock,
             stats.freeStack, stats.totalStack, stats.fragmentation);

  debug("Memory", buffer);
}

void Logger::beginMemoryTracking(const String& sectionName) {
  if (!ConfigMgr.isDebugRAM()) return;

  if (m_currentTracking.isTracking) {
    warning("Memory", F("Previous memory tracking section not closed: ") +
                          m_currentTracking.sectionName);
    endMemoryTracking(m_currentTracking.sectionName);
  }

  m_currentTracking.sectionName = sectionName;
  m_currentTracking.initialStats = getMemoryStats();
  m_currentTracking.isTracking = true;
  m_currentTracking.startTime = millis();

  debug("Memory", readProgmemString(MSG_INITIALIZING) + sectionName);
  logMemoryStats(readProgmemString(MSG_BEFORE));
}

void Logger::endMemoryTracking(const String& sectionName) {
  if (!ConfigMgr.isDebugRAM()) return;

  if (!m_currentTracking.isTracking) {
    warning("Memory", F("No active memory tracking section"));
    return;
  }

  if (sectionName != m_currentTracking.sectionName) {
    warning("Memory", F("Memory tracking section mismatch! Expected: ") +
                          m_currentTracking.sectionName + F(" Got: ") +
                          sectionName);
    return;
  }

  MemoryStats currentStats = getMemoryStats();
  MemoryStats& initialStats = m_currentTracking.initialStats;
  uint32_t duration = millis() - m_currentTracking.startTime;

  int32_t heapDiff = currentStats.freeHeap - initialStats.freeHeap;
  int32_t blockDiff = currentStats.maxFreeBlock - initialStats.maxFreeBlock;
  int32_t stackDiff = currentStats.freeStack - initialStats.freeStack;
  int32_t fragDiff = currentStats.fragmentation - initialStats.fragmentation;

  char buffer[128];
  snprintf_P(buffer, sizeof(buffer), MSG_MEMORY_CHANGES, sectionName.c_str(),
             heapDiff, blockDiff, stackDiff, fragDiff);

  info("Memory", String(buffer) + F(" (") + duration + F("ms)"));
  logMemoryStats(readProgmemString(MSG_AFTER));

  m_currentTracking.isTracking = false;
}

void Logger::enableFileLogging(bool enable) {
  if (enable && !m_fileLoggingEnabled) {
    // Mount filesystem if needed
    if (!LittleFS.exists("/")) {
      if (!LittleFS.begin()) {
        if (m_useSerial) {
          Serial.println(F("Dateisystem konnte beim Aktivieren des Loggings nicht eingehängt werden"));
        }
        return;
      }
    }

    // Create log file if it doesn't exist
    if (!LittleFS.exists(m_logFileName)) {
      File file = LittleFS.open(m_logFileName, "w");
      if (!file || !file.println(F("Logdatei erstellt"))) {
        if (m_useSerial) {
          Serial.println(F("Logdatei konnte nicht erstellt werden"));
        }
        return;
      }
      file.close();
    }

    m_fileLoggingEnabled = true;
    info(F("Logger"), F("Dateilogs aktiviert"));
  } else if (!enable && m_fileLoggingEnabled) {
    m_fileLoggingEnabled = false;
    info(F("Logger"), F("Dateilogs deaktiviert"));
  }
}

bool Logger::isFileLoggingEnabled() const { return m_fileLoggingEnabled; }

void Logger::writeToFile(const String& logMessage) {
  static bool inWriteToFile = false;  // Prevent recursive calls

  if (!m_fileLoggingEnabled || inWriteToFile) {
    return;
  }

  inWriteToFile = true;

  // Check if filesystem is mounted with critical section
  {
    CriticalSection cs;
    if (!LittleFS.exists("/")) {
      if (!LittleFS.begin()) {
        m_fileLoggingEnabled = false;
        if (m_useSerial) {
          Serial.println(F("Dateisystem für Logging konnte nicht eingehängt werden"));
        }
        inWriteToFile = false;
        return;
      }
    }

    File file = LittleFS.open(m_logFileName, "a");
    if (!file) {
      m_fileLoggingEnabled = false;
      if (m_useSerial) {
  Serial.println(F("Logdatei konnte nicht zum Schreiben geöffnet werden"));
      }
      inWriteToFile = false;
      return;
    }

    file.println(logMessage);
    file.close();
  }

  // Only check size occasionally to reduce filesystem operations
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 30000) {  // Check every 30 seconds
    lastCheck = millis();
    truncateLogFileIfNeeded();
  }

  inWriteToFile = false;
}

void Logger::truncateLogFileIfNeeded() {
  if (!m_fileLoggingEnabled) return;

  CriticalSection cs;

  File file = LittleFS.open(m_logFileName, "r");
  if (!file) return;

  if (file.size() <= m_maxFileSize) {
    file.close();
    return;
  }

  // Keep the most recent portion of the file instead of deleting everything.
  // Strategy: copy the last `keepSize` bytes to a temporary file in small
  // chunks, prepend a header that indicates truncation, then replace the
  // original file with the temp file. This avoids allocating a large buffer
  // on the heap (important on ESP8266) and keeps newer log entries.
  size_t fileSize = file.size();
  info(F("Logger"), String(F("Logdatei prüfen: Größe=")) + fileSize + F(" Bytes, Limit=" ) + m_maxFileSize + F(" Bytes"));
  // Try to keep the newer half, but don't exceed the configured maximum
  size_t keepSize = min(fileSize / 2, static_cast<size_t>(m_maxFileSize));
  size_t startPos = (fileSize > keepSize) ? (fileSize - keepSize) : 0;

  String tmpName = String(m_logFileName) + ".tmp";
  File tmp = LittleFS.open(tmpName.c_str(), "w");
  if (!tmp) {
    // If temp file can't be created, fallback to simple truncation
    warning(F("Logger"), F("Temporäre Logdatei konnte nicht erstellt werden, falle auf vollständige Kürzung zurück"));
    file.close();
    LittleFS.remove(m_logFileName);
    File nf = LittleFS.open(m_logFileName, "w");
    if (nf) {
      nf.println(F("Logdatei aufgrund Größenlimit gekürzt"));
      nf.close();
    }
    return;
  }

  // Write header indicating truncation
  tmp.println(F("--- Vorherige Einträge wurden aufgrund des Größenlimits entfernt ---"));

  // Copy the tail of the original file in small chunks
  const size_t BUF_SIZE = 512;
  uint8_t buffer[BUF_SIZE];
  size_t remaining = keepSize;
  file.seek(startPos);
  debug(F("Logger"), String(F("Beginne Kopieren ab Position ")) + startPos + F(" (Bytes zu kopieren: ") + keepSize + F(")"));
  while (remaining > 0) {
    size_t toRead = (remaining > BUF_SIZE) ? BUF_SIZE : remaining;
    size_t r = file.readBytes(reinterpret_cast<char*>(buffer), toRead);
    if (r == 0) break;  // read error or EOF
    tmp.write(buffer, r);
    remaining -= r;
  }

  size_t copied = keepSize - remaining;
  info(F("Logger"), String(F("Kopiert ")) + copied + F(" Bytes in temporäre Datei"));

  file.close();
  tmp.close();

  // Replace original file with temp file. Try to be atomic when possible.
  // Remove original first to ensure rename succeeds on platforms that don't
  // support overwrite-rename.
  LittleFS.remove(m_logFileName);
  if (LittleFS.rename(tmpName.c_str(), m_logFileName)) {
    info(F("Logger"), F("Logdatei erfolgreich gekürzt; ältere Einträge entfernt"));
  } else {
    // Rename failed — try fallback: create a fresh file with header only
    warning(F("Logger"), F("Umbenennen der temporären Logdatei fehlgeschlagen, fallback aktiv"));
    LittleFS.remove(tmpName.c_str());
    File nf = LittleFS.open(m_logFileName, "w");
    if (nf) {
      nf.println(F("Logdatei aufgrund Größenlimit gekürzt"));
      nf.close();
    }
  }
}

String Logger::logLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

String Logger::getIndent(const String& logLevelStr) const {
  const int maxLength = 5;  // Length of the longest log level string
  return String(' ', maxLength - logLevelStr.length());
}

String Logger::logLevelToColor(LogLevel level) const {
  switch (level) {
    case LogLevel::DEBUG:
      return "blue";
    case LogLevel::INFO:
      return "green";
    case LogLevel::WARNING:
      return "orange";
    case LogLevel::ERROR:
      return "red";
    default:
      return "black";
  }
}

String Logger::getFormattedTimestamp() const {
  if (m_ntpInitialized) {
    time_t epochTime = m_timeClient->getEpochTime();
    struct tm* ptm = localtime((time_t*)&epochTime);
    char buffer[32];
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", ptm);
    return String(buffer);
  } else {
    return String(millis() / 1000) + "s";
  }
}

void Logger::initNTP() {
  m_timeClient = new NTPClient(m_ntpUDP, "pool.ntp.org", 0, 60000);
  m_timeClient->begin();
  m_ntpInitialized = true;
  setupTimezone();

  // Debug: Show timezone setup verification
  if (m_useSerial) {
  Serial.println(F("NTP mit Zeitzonenunterstützung initialisiert"));
  }
}

void Logger::setupTimezone() {
  // Set timezone for Berlin (CET/CEST with DST)
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();

  // Debug: Log the timezone setup
  if (m_useSerial) {
  Serial.println(F("Zeitzone auf Berlin (CET/CEST) gesetzt"));
  }
}

void Logger::verifyTimezone() {
  if (!m_ntpInitialized || !m_timeClient) {
    if (m_useSerial) {
  Serial.println(F("NTP nicht initialisiert, Zeitzone kann nicht geprüft werden"));
    }
    return;
  }

  time_t epochTime = m_timeClient->getEpochTime();
  struct tm* utc_time = gmtime(&epochTime);
  struct tm* local_time = localtime(&epochTime);

  char utc_buffer[32];
  char local_buffer[32];
  strftime(utc_buffer, 32, "%Y-%m-%d %H:%M:%S", utc_time);
  strftime(local_buffer, 32, "%Y-%m-%d %H:%M:%S", local_time);

  if (m_useSerial) {
  Serial.print(F("UTC-Zeit: "));
    Serial.println(utc_buffer);
  Serial.print(F("Ortszeit: "));
  Serial.println(local_buffer);
  }
}

void Logger::updateNTP() {
  if (m_ntpInitialized) {
    m_timeClient->update();
  }
}

LogLevel Logger::stringToLogLevel(const String& level) {
  if (level == "DEBUG") return LogLevel::DEBUG;
  if (level == "INFO") return LogLevel::INFO;
  if (level == "WARNING") return LogLevel::WARNING;
  if (level == "ERROR") return LogLevel::ERROR;
  return LogLevel::INFO;  // Default to INFO
}

void Logger::setCallback(
    std::function<void(LogLevel, const String&)> callback) {
  s_logCallback = std::move(callback);
}

std::function<void(LogLevel, const String&)> Logger::getCallback() const {
  return s_logCallback;
}

bool Logger::isCallbackEnabled() const {
  return static_cast<bool>(s_logCallback);
}
