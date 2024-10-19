/**
 * @file logger.h
 * @brief Header file for the Logger class with web support and indented console output
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <vector>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "einstellungen.h"

extern String logLevel;
extern int logAnzahlEintraege;
extern int logAnzahlWebseite;
extern bool logInDatei;

/**
 * @brief Enumeration for different log levels
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Structure to hold a log entry
 */
struct LogEntry {
    LogLevel level;
    String message;
    unsigned long timestamp;
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
     * @param maxEntries Maximum number of log entries to keep in memory
     */
    Logger(LogLevel logLevel = LogLevel::INFO, bool useSerial = true, size_t maxEntries = logAnzahlEintraege, bool fileLoggingEnabled = logInDatei);

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
     * @brief Log a debug message
     * @param message Message to log
     */
    void debug(const String& message);

    /**
     * @brief Log an info message
     * @param message Message to log
     */
    void info(const String& message);

    /**
     * @brief Log a warning message
     * @param message Message to log
     */
    void warning(const String& message);

    /**
     * @brief Log an error message
     * @param message Message to log
     */
    void error(const String& message);

    /**
     * @brief Get log entries as a formatted HTML table based on current log level
     * @param count Number of entries to retrieve for each log level
     * @return Formatted HTML table of log entries
     */
    String getLogsAsHtmlTable(size_t count = logAnzahlWebseite) const;

    /**
     * @brief Initialize NTP client for getting time from the internet
     */
    void initNTP();

    /**
     * @brief Update NTP client to keep time synchronized
     */
    void updateNTP();

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

    /**
     * @brief Get the content of the log file
     * @return Content of the log file
     */
    String getLogFileContent() const;

    /**
     * @brief Clear the log file
     */
    void clearLogFile();

private:
    LogLevel m_logLevel;
    bool m_useSerial;
    size_t m_maxEntries;
    std::vector<LogEntry> m_logEntries;
    WiFiUDP m_ntpUDP;
    NTPClient* m_timeClient;
    bool m_ntpInitialized;
    bool m_fileLoggingEnabled;
    std::array<std::vector<LogEntry>, 4> m_logEntriesByLevel;  // Array of vectors for each log level
    const char* m_logFileName = "/system.log";
    const size_t m_maxFileSize = 100 * 1024;  // 100 KB
    /**
     * @brief Internal method to log a message
     * @param level Log level of the message
     * @param message Message to log
     */
    void log(LogLevel level, const String& message);

    /**
     * @brief Get string representation of log level
     * @param level Log level to convert
     * @return String representation of log level
     */
    String logLevelToString(LogLevel level) const;

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
};

extern Logger logger;

#endif // LOGGER_H
