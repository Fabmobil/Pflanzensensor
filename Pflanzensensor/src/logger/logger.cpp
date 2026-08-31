/**
 * @file Logger.cpp
 * @brief Implementation of the Logger class with web support, indented console
 * output, and file logging
 */

#include "logger.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h> // For timezone support
#ifndef ESP32
#include <umm_malloc/umm_malloc.h>
#endif

#include "configs/config.h"
#include "managers/manager_config.h"
#if USE_WEBSOCKET
#include "web/handler/log_handler.h"
#endif

const char Logger::MSG_MEMORY_STATS[] PROGMEM =
    "Speicher [%s] Heap:%u/%u Block:%u Stack:%u/%u Frag:%u%%";
const char Logger::MSG_FREE_HEAP[] PROGMEM = "- Freier Heap: %u Bytes";
const char Logger::MSG_MAX_FREE_BLOCK[] PROGMEM = "- Größter freier Block: %u Bytes";
const char Logger::MSG_FRAGMENTATION[] PROGMEM = "- Fragmentierung: %u%%";
const char Logger::MSG_FREE_CONT_STACK[] PROGMEM = "- Freier Cont-Stack: %u Bytes";
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
  if (strcmp(LOG_LEVEL, "DEBUG") == 0)
    return LogLevel::DEBUG;
  if (strcmp(LOG_LEVEL, "INFO") == 0)
    return LogLevel::INFO;
  if (strcmp(LOG_LEVEL, "WARNING") == 0)
    return LogLevel::WARNING;
  if (strcmp(LOG_LEVEL, "ERROR") == 0)
    return LogLevel::ERROR;
#endif
  return LogLevel::INFO; // Fallback if not defined or invalid
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
      m_fileLoggingEnabled(false) { // Start false, will be enabled after setup
  if (m_useSerial) {
    Serial.begin(115200);
  }

  // Dateilogging wird hier bewusst NICHT gestartet: der Logger ist ein globales
  // Objekt, sein Konstruktor läuft also vor setup(). LittleFS ist zu diesem
  // Zeitpunkt nicht eingehängt, und Flash-I/O vor dem Hochlauf des SDK ist
  // genau der Zugriff, den diese Klasse sonst vermeidet. Eingeschaltet wird
  // über enableFileLogging(), sobald ConfigManager::loadConfig() gelaufen ist.
  (void)fileLoggingEnabled;
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

  // Präfix als reines Zeichenketten-Literal statt über readProgmemString().
  // Letzteres legte für drei Zeichen einen 128-Byte-Stackpuffer an und baute
  // daraus einen Heap-String - pro ausgegebener Logzeile.
  const char* prefix;
  switch (level) {
  case LogLevel::DEBUG:
    prefix = ".D.";
    break;
  case LogLevel::INFO:
    prefix = ":I:";
    break;
  case LogLevel::WARNING:
    prefix = "!W!";
    break;
  case LogLevel::ERROR:
    prefix = "#E#";
    break;
  default:
    prefix = "???";
    break;
  }

  char formattedMessage[128];
  snprintf(formattedMessage, sizeof(formattedMessage), "%s [%s] %s", prefix, module.c_str(),
           safeMessage.c_str());

  if (m_useSerial) {
    String serialMessage;
    if (m_useColors) {
      // Add simple color codes for better compatibility
      switch (level) {
      case LogLevel::DEBUG:
        serialMessage = "\x1b[90m" + timestamp + " " + formattedMessage + "\x1b[0m"; // Grey
        break;
      case LogLevel::INFO:
        serialMessage = "\x1b[32m" + timestamp + " " + formattedMessage + "\x1b[0m"; // Green
        break;
      case LogLevel::WARNING:
        serialMessage = "\x1b[33m" + timestamp + " " + formattedMessage + "\x1b[0m"; // Orange
        break;
      case LogLevel::ERROR:
        serialMessage = "\x1b[31m" + timestamp + " " + formattedMessage + "\x1b[0m"; // Red
        break;
      }
    } else {
      serialMessage = timestamp + " " + formattedMessage;
    }
    Serial.println(serialMessage);
  }

  if (m_fileLoggingEnabled) {
    bufferForFile(timestamp + " " + formattedMessage);
  }

  // Call the log callback if set
  if (s_logCallback) {
    s_logCallback(level, String(formattedMessage));
  }
}

void Logger::setLogLevel(LogLevel level) {
  m_logLevel = level;
  warning("Logger", String(F("Log-Level gesetzt auf: ")) + logLevelToString(level));
}

LogLevel Logger::getLogLevel() const { return m_logLevel; }

MemoryStats Logger::getMemoryStats() {
  MemoryStats stats;
  stats.freeHeap = ESP.getFreeHeap();

#ifdef ESP32
  stats.maxFreeBlock = ESP.getMaxAllocHeap();
  stats.fragmentation = 0; // ESP32 doesn't have getHeapFragmentation
  stats.freeContStack = 0; // Not available on ESP32
  stats.freeStack = uxTaskGetStackHighWaterMark(NULL);
  stats.totalHeap = ESP.getHeapSize();
  stats.totalStack = CONFIG_ARDUINO_LOOP_STACK_SIZE;
#else
  stats.maxFreeBlock = ESP.getMaxFreeBlockSize();
  stats.fragmentation = static_cast<uint8_t>(ESP.getHeapFragmentation());
  stats.freeContStack = ESP.getFreeContStack();
  stats.freeStack = ESP.getFreeHeap() - ESP.getMaxFreeBlockSize();
  stats.totalHeap = 81920; // ESP8266 typically has 80KB heap
  stats.totalStack = ESP.getFreeContStack() + (ESP.getFreeHeap() - ESP.getMaxFreeBlockSize());
#endif

  // Update peak values
  updatePeakStats(stats);

  return stats;
}

void Logger::updatePeakStats(const MemoryStats& stats) {
  m_peakStats.minFreeHeap = min(m_peakStats.minFreeHeap, stats.freeHeap);
  m_peakStats.minFreeBlock = min(m_peakStats.minFreeBlock, stats.maxFreeBlock);
  m_peakStats.maxFragmentation = max(m_peakStats.maxFragmentation, stats.fragmentation);
}

void Logger::logMemoryStats(const String& location) {
  if (!ConfigMgr.isDebugRAM())
    return;
  MemoryStats stats = getMemoryStats();

  char buffer[128];
  snprintf_P(buffer, sizeof(buffer), MSG_MEMORY_STATS, location.c_str(), stats.freeHeap,
             stats.totalHeap, stats.maxFreeBlock, stats.freeStack, stats.totalStack,
             stats.fragmentation);

  debug("Memory", buffer);
}

void Logger::beginMemoryTracking(const String& sectionName) {
  if (!ConfigMgr.isDebugRAM())
    return;

  if (m_currentTracking.isTracking) {
    warning("Memory", String(F("Previous memory tracking section not closed: ")) +
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
  if (!ConfigMgr.isDebugRAM())
    return;

  if (!m_currentTracking.isTracking) {
    warning("Memory", F("No active memory tracking section"));
    return;
  }

  if (sectionName != m_currentTracking.sectionName) {
    warning("Memory", String(F("Memory tracking section mismatch! Expected: ")) +
                          m_currentTracking.sectionName + String(F(" Got: ")) + sectionName);
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
  snprintf_P(buffer, sizeof(buffer), MSG_MEMORY_CHANGES, sectionName.c_str(), heapDiff, blockDiff,
             stackDiff, fragDiff);

  info("Memory", String(buffer) + String(F(" (")) + String(duration) + String(F("ms)")));
  logMemoryStats(readProgmemString(MSG_AFTER));

  m_currentTracking.isTracking = false;
}

void Logger::enableFileLogging(bool enable) {
  // Kein Flash-Zugriff an dieser Stelle: der Aufruf kommt unter anderem aus
  // einem Web-Handler mitten in der Anfragebearbeitung. Eingehängt wird erst
  // beim ersten Schreiben in flushFileLog().
  if (enable && !m_fileLoggingEnabled) {
    m_fileBuffer = "";
    m_fileBuffer.reserve(FILE_FLUSH_THRESHOLD + 128);
    m_droppedLines = 0;
    m_lastFlush = millis();
    m_fileLoggingEnabled = true;
    info(F("Logger"), F("Dateilogs aktiviert"));
  } else if (!enable && m_fileLoggingEnabled) {
    m_fileLoggingEnabled = false;
    m_fileBuffer = "";
    info(F("Logger"), F("Dateilogs deaktiviert"));
  }
}

bool Logger::isFileLoggingEnabled() const { return m_fileLoggingEnabled; }

bool Logger::ensureFilesystem() {
  if (m_fsMounted)
    return true;
  if (!LittleFS.begin()) {
    if (m_useSerial) {
      Serial.println(F("Dateisystem für Logging konnte nicht eingehängt werden"));
    }
    return false;
  }
  m_fsMounted = true;
  return true;
}

void Logger::bufferForFile(const String& logMessage) {
  if (!m_fileLoggingEnabled)
    return;

  if (m_fileBuffer.length() + logMessage.length() + 2 > FILE_BUFFER_MAX) {
    // Puffer voll. Hier NICHT in den Flash schreiben: log() ist aus beliebigem
    // Kontext erreichbar, auch aus einem SDK-Callback, und ein Flash-Schreiben
    // von dort legt den WiFi-Stack lahm. Stattdessen Zeile verwerfen und den
    // Verlust in flushFileLog() vermerken.
    if (m_droppedLines < 0xFFFF)
      m_droppedLines++;
    return;
  }

  m_fileBuffer += logMessage;
  m_fileBuffer += '\n';
}

void Logger::flushFileLog() {
  static bool inFlush = false; // Prevent recursive calls

  if (!m_fileLoggingEnabled || inFlush || m_fileBuffer.length() == 0) {
    return;
  }

  // Nur schreiben, wenn genug aufgelaufen ist oder das Intervall abgelaufen ist
  if (m_fileBuffer.length() < FILE_FLUSH_THRESHOLD &&
      millis() - m_lastFlush < FILE_FLUSH_INTERVAL_MS) {
    return;
  }

  inFlush = true;
  m_lastFlush = millis();

  // Bewusst OHNE CriticalSection: SPI-Flash-Schreibvorgänge brauchen
  // eingeschaltete Interrupts. Werden sie über einen LittleFS-Commit hinweg
  // maskiert, verhungert der SDK-/WiFi-Task - Exception 2 im sys-Kontext mit
  // anschließendem WDT-Reset.
  if (!ensureFilesystem()) {
    m_fileLoggingEnabled = false;
    m_fileBuffer = "";
    inFlush = false;
    return;
  }

  File file = LittleFS.open(m_logFileName, "a");
  if (!file) {
    m_fileLoggingEnabled = false;
    m_fileBuffer = "";
    if (m_useSerial) {
      Serial.println(F("Logdatei konnte nicht zum Schreiben geöffnet werden"));
    }
    inFlush = false;
    return;
  }

  if (m_droppedLines > 0) {
    file.print(F("--- "));
    file.print(m_droppedLines);
    file.println(F(" Logzeilen verworfen (Puffer voll) ---"));
    m_droppedLines = 0;
  }

  file.print(m_fileBuffer);
  file.close();
  m_fileBuffer = "";
  yield();

  // Only check size occasionally to reduce filesystem operations
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 60000) { // Check every 60 seconds
    lastCheck = millis();
    truncateLogFileIfNeeded();
  }

  inFlush = false;
}

void Logger::truncateLogFileIfNeeded() {
  if (!m_fileLoggingEnabled || !m_fsMounted)
    return;

  File file = LittleFS.open(m_logFileName, "r");
  if (!file)
    return;

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
  if (m_useSerial) {
    Serial.print(F("Logdatei wird gekürzt, Größe="));
    Serial.println(fileSize);
  }
  // Try to keep the newer half, but don't exceed the configured maximum
  size_t keepSize = min(fileSize / 2, static_cast<size_t>(m_maxFileSize));
  size_t startPos = (fileSize > keepSize) ? (fileSize - keepSize) : 0;

  String tmpName = String(m_logFileName) + ".tmp";
  File tmp = LittleFS.open(tmpName.c_str(), "w");
  if (!tmp) {
    // If temp file can't be created, fallback to simple truncation
    if (m_useSerial) {
      Serial.println(F("Temporäre Logdatei fehlgeschlagen, kürze vollständig"));
    }
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

  // Copy the tail of the original file in small chunks, yielding between
  // chunks so the SDK/WiFi task and the software watchdog stay serviced.
  const size_t BUF_SIZE = 256;
  uint8_t buffer[BUF_SIZE];
  size_t remaining = keepSize;
  file.seek(startPos);
  while (remaining > 0) {
    size_t toRead = (remaining > BUF_SIZE) ? BUF_SIZE : remaining;
    size_t r = file.readBytes(reinterpret_cast<char*>(buffer), toRead);
    if (r == 0)
      break; // read error or EOF
    tmp.write(buffer, r);
    remaining -= r;
    yield();
  }

  file.close();
  tmp.close();

  // Replace original file with temp file. Try to be atomic when possible.
  // Remove original first to ensure rename succeeds on platforms that don't
  // support overwrite-rename.
  LittleFS.remove(m_logFileName);
  if (!LittleFS.rename(tmpName.c_str(), m_logFileName)) {
    // Rename failed — try fallback: create a fresh file with header only
    if (m_useSerial) {
      Serial.println(F("Umbenennen der temporären Logdatei fehlgeschlagen"));
    }
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
  // Zweimal aufrufen würde den vorigen Client verlieren, ohne ihn freizugeben.
  // Das passiert seit der stündlichen Nachführung in loop() nicht mehr von
  // allein, aber der Aufruf ist von außen erreichbar - also hier abfangen.
  if (m_ntpInitialized) {
    return;
  }

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
  if (level == "DEBUG")
    return LogLevel::DEBUG;
  if (level == "INFO")
    return LogLevel::INFO;
  if (level == "WARNING")
    return LogLevel::WARNING;
  if (level == "ERROR")
    return LogLevel::ERROR;
  return LogLevel::INFO; // Default to INFO
}

void Logger::setCallback(std::function<void(LogLevel, const String&)> callback) {
  s_logCallback = std::move(callback);
}

std::function<void(LogLevel, const String&)> Logger::getCallback() const { return s_logCallback; }

bool Logger::isCallbackEnabled() const { return static_cast<bool>(s_logCallback); }
