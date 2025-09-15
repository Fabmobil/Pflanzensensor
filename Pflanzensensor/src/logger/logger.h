/**
 * @file logger.h
 * @brief Header file for the Logger class with web support and indented console
 * output
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <Esp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <cstring>
#include <functional>
#include <vector>

#include "configs/config.h"
/**
 * @brief Enumeration for different log levels
 */
enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

/**
 * @brief Structure to hold a log entry
 */
struct LogEntry {
  LogLevel level;
  char message[128];  // Fixed-size buffer for log message
  unsigned long timestamp;
};

/**
 * @brief Structure to hold memory statistics
 */
struct MemoryStats {
  uint32_t freeHeap;
  uint32_t maxFreeBlock;
  uint8_t fragmentation;
  uint32_t freeContStack;
  uint32_t freeStack;
  uint32_t totalHeap;   // Total heap size
  uint32_t totalStack;  // Total stack size
};

/**
 * @brief Structure to hold peak memory statistics
 */
struct PeakMemoryStats {
  uint32_t minFreeHeap = UINT32_MAX;
  uint32_t minFreeBlock = UINT32_MAX;
  uint8_t maxFragmentation = 0;
};

/**
 * @brief Structure to hold memory tracking state
 */
struct MemoryTrackingState {
  String sectionName;
  MemoryStats initialStats;
  bool isTracking = false;
  uint32_t startTime = 0;
};

/**
 * @brief Logger class for handling log messages
 */
class Logger {
 public:
  /**
   * @brief Constructor for Logger class
   * @param logLevel Minimum log level to display
   * @param useSerial Whether to output logs to Serial
   * @param fileLoggingEnabled Whether to enable file logging
   */
  Logger(LogLevel logLevel = LogLevel::INFO, bool useSerial = true,
         bool fileLoggingEnabled = FILE_LOGGING_ENABLED);

  /**
   * @brief Set the log level
   * @param level The new log level
   */
  void setLogLevel(LogLevel level);

  /**
   * @brief Get the current log level
   * @return The current log level
   */
  LogLevel getLogLevel() const;

  /**
   * @brief Log a debug message with module name
   * @param module Module name that will be shown in brackets
   * @param message Message to log
   */
  void debug(const String& module, const String& message);

  /**
   * @brief Log an info message with module name
   * @param module Module name that will be shown in brackets
   * @param message Message to log
   */
  void info(const String& module, const String& message);

  /**
   * @brief Log a warning message with module name
   * @param module Module name that will be shown in brackets
   * @param message Message to log
   */
  void warning(const String& module, const String& message);

  /**
   * @brief Log an error message with module name
   * @param module Module name that will be shown in brackets
   * @param message Message to log
   */
  void error(const String& module, const String& message);

  /**
   * @brief Get detailed memory statistics
   * @return MemoryStats structure with current memory information
   */
  MemoryStats getMemoryStats();

  /**
   * @brief Log current memory statistics with detailed information
   * @param location Optional location/context string for the memory check
   */
  void logMemoryStats(const String& location = "");

  /**
   * @brief Start tracking memory for a critical section
   * @param sectionName Name of the critical section
   */
  void beginMemoryTracking(const String& sectionName);

  /**
   * @brief End tracking memory for a critical section and log changes
   * @param sectionName Name of the critical section
   */
  void endMemoryTracking(const String& sectionName);

  /**
   * @brief Initialize NTP client for getting time from the internet
   */
  void initNTP();

  /**
   * @brief Update NTP client to keep time synchronized
   */
  void updateNTP();

  /**
   * @brief Setup timezone for Berlin (CET/CEST with DST)
   */
  void setupTimezone();

  /**
   * @brief Verify timezone setup by showing both UTC and local time
   */
  void verifyTimezone();

  /**
   * @brief Enable or disable file logging
   * @param enable True to enable file logging, false to disable
   */
  void enableFileLogging(bool enable);

  /**
   * @brief Check if file logging is enabled
   * @return True if file logging is enabled, false otherwise
   */
  bool isFileLoggingEnabled() const;

  bool isNTPInitialized() const {
    return m_ntpInitialized && m_timeClient != nullptr;
  }

  time_t getSynchronizedTime() const {
    if (m_ntpInitialized && m_timeClient) {
      return m_timeClient->getEpochTime();
    }
    return 0;
  }

  /**
   * @brief Converts a string representation of log level to LogLevel enum
   * @param level String representation of log level ("DEBUG", "INFO", etc)
   * @return Corresponding LogLevel enum value, defaults to INFO if invalid
   */
  static LogLevel stringToLogLevel(const String& level);

  /**
   * @brief Get string representation of log level
   * @param level Log level to convert
   * @return String representation of log level
   */
  static String logLevelToString(LogLevel level);

  /**
   * @brief Initialize WebSocket server for real-time logging
   */
  void initWebSocket();

  /**
   * @brief Set a callback to be called on every new log message
   * @param callback Function to call with log level and message
   */
  void setCallback(std::function<void(LogLevel, const String&)> callback);

  /**
   * @brief Get the current callback function
   * @return The current callback function
   */
  std::function<void(LogLevel, const String&)> getCallback() const;

  /**
   * @brief Check if the callback is enabled (not null)
   * @return True if callback is enabled, false otherwise
   */
  bool isCallbackEnabled() const;

 private:
  LogLevel m_logLevel;
  bool m_useSerial;
  bool m_useColors;
  WiFiUDP m_ntpUDP;
  NTPClient* m_timeClient;
  bool m_ntpInitialized;
  bool m_fileLoggingEnabled;
  const char* m_logFileName = "/log.txt";
  const size_t m_maxFileSize = 100 * 1024;  // 100 KB
  unsigned long lastErrorLogTime = 0;
  const unsigned long errorLogInterval = 5000;  // 5 seconds
  int errorCount = 0;

  // Memory tracking
  PeakMemoryStats m_peakStats;
  MemoryTrackingState m_currentTracking;

  /**
   * @brief Internal method to log a message with module name
   * @param level Log level of the message
   * @param module Module name that will be shown in brackets
   * @param message Message to log
   */
  void log(LogLevel level, const String& module, const String& message);

  /**
   * @brief Get indentation for log level
   * @param logLevelStr String representation of log level
   * @return Indentation string
   */
  String getIndent(const String& logLevelStr) const;

  /**
   * @brief Get color representation of log level for HTML
   * @param level Log level to convert
   * @return Color string for HTML
   */
  String logLevelToColor(LogLevel level) const;

  /**
   * @brief Get current timestamp as a formatted string
   * @return Formatted timestamp string
   */
  String getFormattedTimestamp() const;

  /**
   * @brief Write a log message to the log file
   * @param logMessage Message to write
   */
  void writeToFile(const String& logMessage);

  /**
   * @brief Truncate the log file if it exceeds the maximum size
   */
  void truncateLogFileIfNeeded();

  static inline String readProgmemString(const char* progmem_str) {
    char buffer[128];  // Adjust size as needed
    strcpy_P(buffer, progmem_str);
    return String(buffer);
  }

  static const char PROGMEM MSG_MEMORY_STATS[];
  static const char PROGMEM MSG_FREE_HEAP[];
  static const char PROGMEM MSG_MAX_FREE_BLOCK[];
  static const char PROGMEM MSG_FRAGMENTATION[];
  static const char PROGMEM MSG_FREE_CONT_STACK[];
  static const char PROGMEM MSG_FREE_STACK[];
  static const char PROGMEM MSG_BYTES[];
  static const char PROGMEM MSG_INITIALIZING[];
  static const char PROGMEM MSG_SUCCESS[];
  static const char PROGMEM MSG_ERROR[];
  static const char PROGMEM MSG_WARNING[];
  static const char PROGMEM MSG_INFO[];
  static const char PROGMEM MSG_DEBUG[];
  static const char PROGMEM MSG_BEFORE[];
  static const char PROGMEM MSG_AFTER[];
  static const char PROGMEM MSG_MEMORY_CHANGES[];

  /**
   * @brief Update peak memory statistics with current values
   * @param stats Current memory statistics
   */
  void updatePeakStats(const MemoryStats& stats);
};

extern Logger logger;

#endif  // LOGGER_H
