/**
 * @file Logger.cpp
 * @brief Implementation of the Logger class with web support, indented console output, and file logging
 */

#include <LittleFS.h>
#include "logger.h"
#include "config.h"

Logger logger(LogLevel::DEBUG, true, logAnzahlEintraege, logInDatei);

Logger::Logger(LogLevel logLevel, bool useSerial, size_t maxEntries, bool fileLoggingEnabled)
  : m_logLevel(logLevel),
    m_useSerial(useSerial),
    m_maxEntries(maxEntries),
    m_timeClient(nullptr),
    m_ntpInitialized(false),
    m_fileLoggingEnabled(fileLoggingEnabled) {
  if (m_useSerial) {
    Serial.begin(115200);
  }
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
  }

  // Initialize the log entries vectors
  for (int i = 0; i < 4; ++i) {
    m_logEntriesByLevel[i].reserve(logAnzahlWebseite);
  }
}
void Logger::setLogLevel(LogLevel level) {
  m_logLevel = level;
  debug("Log level set to: " + logLevelToString(level));
}

LogLevel Logger::getLogLevel() const {
  return m_logLevel;
}

void Logger::debug(const String& message) {
  log(LogLevel::DEBUG, message);
}

void Logger::info(const String& message) {
  log(LogLevel::INFO, message);
}

void Logger::warning(const String& message) {
  log(LogLevel::WARNING, message);
}

void Logger::error(const String& message) {
  log(LogLevel::ERROR, message);
}

void Logger::enableFileLogging(bool enable) {
  if (enable && !LittleFS.exists(m_logFileName)) {
    File file = LittleFS.open(m_logFileName, "w");
    if (file) {
      file.println("Log file created");
      file.close();
    } else {
      Serial.println("Failed to create log file");
      return;
    }
  }

  m_fileLoggingEnabled = enable;
  if (enable) {
    log(LogLevel::INFO, "File logging enabled");
  } else {
    log(LogLevel::INFO, "File logging disabled");
  }
}

bool Logger::isFileLoggingEnabled() const {
  return m_fileLoggingEnabled;
}

String Logger::getLogFileContent() const {
  if (!m_fileLoggingEnabled) {
    return "File logging is disabled";
  }

  if (!LittleFS.exists(m_logFileName)) {
    return "Log file does not exist";
  }

  File file = LittleFS.open(m_logFileName, "r");
  if (!file) {
    return "Failed to open log file";
  }

  String content = file.readString();
  file.close();
  return content;
}

void Logger::clearLogFile() {
  if (!m_fileLoggingEnabled) {
    return;
  }

  File file = LittleFS.open(m_logFileName, "w");
  if (file) {
    file.close();
  }
}

void Logger::log(LogLevel level, const String& message) {
  if (level < m_logLevel) {
    return;
  }

  String timestamp = getFormattedTimestamp();
  LogEntry entry = {level, message, m_ntpInitialized ? m_timeClient->getEpochTime() : millis()};

  // Add to the appropriate vector based on log level
  m_logEntriesByLevel[static_cast<int>(level)].push_back(entry);

  // Keep only the latest logAnzahlWebseite entries for each level
  if (m_logEntriesByLevel[static_cast<int>(level)].size() > logAnzahlWebseite) {
    m_logEntriesByLevel[static_cast<int>(level)].erase(m_logEntriesByLevel[static_cast<int>(level)].begin());
  }

  String logLevelStr = logLevelToString(level);
  String indent = getIndent(logLevelStr);
  String formattedMessage = timestamp + " " + indent + logLevelStr + ": " + message;

  if (m_useSerial) {
    Serial.println(formattedMessage);
  }

  if (m_fileLoggingEnabled) {
    writeToFile(formattedMessage);
  }
}

void Logger::writeToFile(const String& logMessage) {
  if (!m_fileLoggingEnabled || !LittleFS.exists(m_logFileName)) {
    return;
  }

  File file = LittleFS.open(m_logFileName, "a");
  if (file) {
    file.println(logMessage);
    file.close();
    truncateLogFileIfNeeded();
  } else {
    Serial.println("Failed to open log file for writing");
  }
}

void Logger::truncateLogFileIfNeeded() {
  File file = LittleFS.open(m_logFileName, "r");
  if (file) {
    if (file.size() > m_maxFileSize) {
      String content = file.readString();
      file.close();

      int cutIndex = content.indexOf("\n", content.length() - m_maxFileSize);
      if (cutIndex != -1) {
        content = content.substring(cutIndex + 1);
        File writeFile = LittleFS.open(m_logFileName, "w");
        if (writeFile) {
          writeFile.print(content);
          writeFile.close();
        }
      }
    } else {
      file.close();
    }
  }
}

String Logger::logLevelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::DEBUG:   return "DEBUG";
    case LogLevel::INFO:    return "INFO ";
    case LogLevel::WARNING: return "WARN ";
    case LogLevel::ERROR:   return "ERROR";
    default:                return "UNKNOWN";
  }
}

String Logger::getIndent(const String& logLevelStr) const {
  const int maxLength = 5; // Length of the longest log level string
  return String(' ', maxLength - logLevelStr.length());
}

String Logger::logLevelToColor(LogLevel level) const {
  switch (level) {
    case LogLevel::DEBUG:   return "blue";
    case LogLevel::INFO:    return "green";
    case LogLevel::WARNING: return "orange";
    case LogLevel::ERROR:   return "red";
    default:                return "black";
  }
}

String Logger::getLogsAsHtmlTable(size_t count) const {
  String html = "<table class='log'><tr><th>Time</th><th>Level</th><th>Message</th></tr>";

  std::vector<LogEntry> visibleEntries;

  // Collect visible entries based on current log level
  for (int i = static_cast<int>(m_logLevel); i < 4; ++i) {
    visibleEntries.insert(visibleEntries.end(), m_logEntriesByLevel[i].begin(), m_logEntriesByLevel[i].end());
  }

  // Sort entries by timestamp (descending order)
  std::sort(visibleEntries.begin(), visibleEntries.end(),
    [](const LogEntry& a, const LogEntry& b) { return a.timestamp > b.timestamp; });

  // Limit to count entries
  size_t entriesToShow = std::min(count, visibleEntries.size());

  for (size_t i = 0; i < entriesToShow; ++i) {
    const auto& entry = visibleEntries[i];
    String timestamp;
    if (m_ntpInitialized) {
      time_t epochTime = entry.timestamp;
      struct tm *ptm = gmtime ((time_t *)&epochTime);
      char buffer[32];
      strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", ptm);
      timestamp = String(buffer);
    } else {
      timestamp = String(entry.timestamp / 1000) + "s";
    }
    html += "<tr>"
            "<td>" + timestamp + "</td>"
            "<td style='color: " + logLevelToColor(entry.level) + ";'>" + logLevelToString(entry.level) + "</td>"
            "<td>" + entry.message + "</td>"
            "</tr>";
  }

  html += "</table>";
  return html;
}

String Logger::getFormattedTimestamp() const {
  if (m_ntpInitialized) {
    time_t epochTime = m_timeClient->getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    char buffer[32];
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", ptm);
    return String(buffer);
  } else {
    return String(millis() / 1000) + "s";
  }
}

void Logger::initNTP() {
  m_timeClient = new NTPClient(m_ntpUDP, "pool.ntp.org", 3600, 60000);
  m_timeClient->begin();
  m_ntpInitialized = true;
}

void Logger::updateNTP() {
  if (m_ntpInitialized) {
    m_timeClient->update();
  }
}
