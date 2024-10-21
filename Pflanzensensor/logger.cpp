/**
 * @file Logger.cpp
 * @brief Implementierung der Logger-Klasse mit Webunterstützung, eingerückter Konsolenausgabe und Datei-Logging
 */

#include <LittleFS.h>
#include "logger.h"
#include "einstellungen.h"

Logger logger(LogLevel::DEBUG, true, logAnzahlEintraege, logInDatei);

// Konstante Strings im Flash-Speicher
const char LOG_LEVEL_DEBUG[] PROGMEM = "DEBUG";
const char LOG_LEVEL_INFO[] PROGMEM = "INFO ";
const char LOG_LEVEL_WARN[] PROGMEM = "WARN ";
const char LOG_LEVEL_ERROR[] PROGMEM = "ERROR";
const char LOG_LEVEL_UNKNOWN[] PROGMEM = "UNKNOWN";
const char* const LOG_LEVEL_STRINGS[] PROGMEM = {LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR, LOG_LEVEL_UNKNOWN};

const char LOG_COLOR_BLUE[] PROGMEM = "blue";
const char LOG_COLOR_GREEN[] PROGMEM = "green";
const char LOG_COLOR_ORANGE[] PROGMEM = "orange";
const char LOG_COLOR_RED[] PROGMEM = "red";
const char LOG_COLOR_BLACK[] PROGMEM = "black";
const char* const LOG_COLORS[] PROGMEM = {LOG_COLOR_BLUE, LOG_COLOR_GREEN, LOG_COLOR_ORANGE, LOG_COLOR_RED, LOG_COLOR_BLACK};

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
    Serial.println(F("Fehler: Dateisystem konnte nicht eingebunden werden"));
  }

  // Initialisierung der Log-Einträge
  for (int i = 0; i < 4; ++i) {
    m_logEntriesByLevel[i].fill(LogEntry());
  }
}

void Logger::setLogLevel(LogLevel level) {
  m_logLevel = level;
  debug(F("Log-Level gesetzt auf: ") + String(FPSTR(LOG_LEVEL_STRINGS[static_cast<int>(level)])));
}

LogLevel Logger::getLogLevel() const {
  return m_logLevel;
}

void Logger::debug(const String& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const String& message) { log(LogLevel::INFO, message); }
void Logger::warning(const String& message) { log(LogLevel::WARNING, message); }
void Logger::error(const String& message) { log(LogLevel::ERROR, message); }

void Logger::enableFileLogging(bool enable) {
  if (enable && !LittleFS.exists(F("/system.log"))) {
    File file = LittleFS.open(F("/system.log"), "w");
    if (file) {
      file.println(F("Log-Datei erstellt"));
      file.close();
    } else {
      Serial.println(F("Fehler: Log-Datei konnte nicht erstellt werden"));
      return;
    }
  }

  m_fileLoggingEnabled = enable;
  log(LogLevel::INFO, enable ? F("Datei-Logging aktiviert") : F("Datei-Logging deaktiviert"));
}

bool Logger::isFileLoggingEnabled() const {
  return m_fileLoggingEnabled;
}

String Logger::getLogFileContent() const {
  if (!m_fileLoggingEnabled) return F("Datei-Logging ist deaktiviert");
  if (!LittleFS.exists(F("/system.log"))) return F("Log-Datei existiert nicht");

  File file = LittleFS.open(F("/system.log"), "r");
  if (!file) return F("Fehler beim Öffnen der Log-Datei");

  String content;
  while (file.available()) {
    content += file.readStringUntil('\n') + '\n';
  }
  file.close();
  return content;
}

void Logger::clearLogFile() {
  if (!m_fileLoggingEnabled) return;
  File file = LittleFS.open(F("/system.log"), "w");
  if (file) file.close();
}

void Logger::log(LogLevel level, const String& message) {
  if (level < m_logLevel) return;

  char timestampBuffer[32];
  char logMessageBuffer[256];

  getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer));

  LogEntry entry = {level, message, m_ntpInitialized ? m_timeClient->getEpochTime() : millis()};

  // Füge zum entsprechenden Array basierend auf Log-Level hinzu
  size_t levelIndex = static_cast<size_t>(level);
  if (levelIndex < m_logEntriesByLevel.size()) {
    auto& levelEntries = m_logEntriesByLevel[levelIndex];
    static size_t currentIndex = 0;
    levelEntries[currentIndex] = entry;
    currentIndex = (currentIndex + 1) % MAX_LOG_ENTRIES;
  }

  const char* logLevelStr = reinterpret_cast<const char*>(FPSTR(LOG_LEVEL_STRINGS[static_cast<int>(level)]));
  int indent = 5 - strlen(logLevelStr);

  snprintf_P(logMessageBuffer, sizeof(logMessageBuffer), PSTR("%s %*s%s: %s"),
             timestampBuffer, indent, "", logLevelStr, message.c_str());

  if (m_useSerial) {
    Serial.println(logMessageBuffer);
  }

  if (m_fileLoggingEnabled) {
    writeToFile(logMessageBuffer);
  }
}

void Logger::writeToFile(const char* logMessage) {
  if (!m_fileLoggingEnabled) return;

  File file = LittleFS.open(F("/system.log"), "a");
  if (file) {
    file.println(logMessage);
    file.close();
    truncateLogFileIfNeeded();
  } else {
    Serial.println(F("Fehler beim Öffnen der Log-Datei zum Schreiben"));
  }
}

void Logger::truncateLogFileIfNeeded() {
  File file = LittleFS.open(F("/system.log"), "r");
  if (file) {
    if (file.size() > 100 * 1024) {  // 100 KB
      String content = file.readString();
      file.close();

      int cutIndex = content.indexOf("\n", content.length() - 100 * 1024);
      if (cutIndex != -1) {
        content = content.substring(cutIndex + 1);
        File writeFile = LittleFS.open(F("/system.log"), "w");
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

String Logger::getLogsAsHtmlTable(size_t count) const {
  String html = F("<table class='log'><tr><th>Zeit</th><th>Level</th><th>Nachricht</th></tr>");

  std::array<LogEntry, MAX_LOG_ENTRIES * 4> allEntries;
  size_t totalEntries = 0;

  for (size_t i = static_cast<size_t>(m_logLevel); i < m_logEntriesByLevel.size(); ++i) {
    const auto& levelEntries = m_logEntriesByLevel[i];
    for (const auto& entry : levelEntries) {
      if (entry.timestamp != 0 && totalEntries < allEntries.size()) {
        allEntries[totalEntries++] = entry;
      }
    }
  }

  std::sort(allEntries.begin(), allEntries.begin() + totalEntries,
    [](const LogEntry& a, const LogEntry& b) { return a.timestamp > b.timestamp; });

  size_t entriesToShow = std::min(count, totalEntries);

  char timestampBuffer[32];
  for (size_t i = 0; i < entriesToShow; ++i) {
    const auto& entry = allEntries[i];
    getFormattedTimestamp(timestampBuffer, sizeof(timestampBuffer));
    html += F("<tr><td>");
    html += timestampBuffer;
    html += F("</td><td style='color:");
    html += FPSTR(LOG_COLORS[static_cast<int>(entry.level)]);
    html += F("'>");
    html += FPSTR(LOG_LEVEL_STRINGS[static_cast<int>(entry.level)]);
    html += F("</td><td>");
    html += entry.message;
    html += F("</td></tr>");
  }

  html += F("</table>");
  return html;
}

void Logger::getFormattedTimestamp(char* buffer, size_t bufferSize) const {
  if (m_ntpInitialized) {
    time_t epochTime = m_timeClient->getEpochTime();
    struct tm *ptm = gmtime(&epochTime);
    strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", ptm);
  } else {
    unsigned long t = millis();
    snprintf_P(buffer, bufferSize, PSTR("%lus"), t / 1000);
  }
}

void Logger::initNTP() {
  m_timeClient = new NTPClient(m_ntpUDP, "pool.ntp.org", 3600, 60000);
  m_timeClient->begin();
  m_ntpInitialized = true;
}

void Logger::updateNTP() {
  if (m_ntpInitialized && m_timeClient) {
    m_timeClient->update();
  }
}
