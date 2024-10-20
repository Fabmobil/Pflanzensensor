/**
 * @file Logger.cpp
 * @brief Implementierung der Logger-Klasse mit Webunterstützung, eingerückter Konsolenausgabe und Datei-Logging
 */

#include <LittleFS.h>
#include "logger.h"
#include "einstellungen.h"

Logger logger(LogLevel::DEBUG, true, logAnzahlEintraege, logInDatei);

// Konstante Strings im Flash-Speicher
const char* const LOG_LEVEL_STRINGS[] PROGMEM = {"DEBUG", "INFO ", "WARN ", "ERROR", "UNKNOWN"};
const char* const LOG_COLORS[] PROGMEM = {"blue", "green", "orange", "red", "black"};

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
  debug(F("Log-Level gesetzt auf: ") + String(LOG_LEVEL_STRINGS[static_cast<int>(level)]));
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
      file.println(F("Log-Datei erstellt"));
      file.close();
    } else {
      Serial.println(F("Fehler: Log-Datei konnte nicht erstellt werden"));
      return;
    }
  }

  m_fileLoggingEnabled = enable;
  if (enable) {
    log(LogLevel::INFO, F("Datei-Logging aktiviert"));
  } else {
    log(LogLevel::INFO, F("Datei-Logging deaktiviert"));
  }
}

bool Logger::isFileLoggingEnabled() const {
  return m_fileLoggingEnabled;
}

String Logger::getLogFileContent() const {
  if (!m_fileLoggingEnabled) {
    return F("Datei-Logging ist deaktiviert");
  }

  if (!LittleFS.exists(m_logFileName)) {
    return F("Log-Datei existiert nicht");
  }

  File file = LittleFS.open(m_logFileName, "r");
  if (!file) {
    return F("Fehler beim Öffnen der Log-Datei");
  }

  String content;
  while (file.available()) {
    content += file.readStringUntil('\n') + '\n';
  }
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

  const char* logLevelStr = LOG_LEVEL_STRINGS[static_cast<int>(level)];
  String indent = getIndent(logLevelStr);

  snprintf(logMessageBuffer, sizeof(logMessageBuffer), "%s %s%s: %s",
           timestampBuffer, indent.c_str(), logLevelStr, message.c_str());

  if (m_useSerial) {
    Serial.println(logMessageBuffer);
  }

  if (m_fileLoggingEnabled) {
    writeToFile(logMessageBuffer);
  }
}

void Logger::writeToFile(const char* logMessage) {
  if (!m_fileLoggingEnabled || !LittleFS.exists(m_logFileName)) {
    return;
  }

  File file = LittleFS.open(m_logFileName, "a");
  if (file) {
    file.println(logMessage);
    file.close();
    truncateLogFileIfNeeded();
  } else {
    Serial.println(F("Fehler beim Öffnen der Log-Datei zum Schreiben"));
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

String Logger::getIndent(const char* logLevelStr) const {
  const int maxLength = 5; // Länge des längsten Log-Level-Strings
  return String(' ', maxLength - strlen(logLevelStr));
}

String Logger::getLogsAsHtmlTable(size_t count) const {
  String html = F("<table class='log'><tr><th>Zeit</th><th>Level</th><th>Nachricht</th></tr>");

  std::vector<LogEntry> visibleEntries;

  // Sammle sichtbare Einträge basierend auf aktuellem Log-Level
  for (size_t i = static_cast<size_t>(m_logLevel); i < m_logEntriesByLevel.size(); ++i) {
    const auto& levelEntries = m_logEntriesByLevel[i];
    for (const auto& entry : levelEntries) {
      if (entry.timestamp != 0) { // Überprüfe, ob der Eintrag gültig ist
        visibleEntries.push_back(entry);
      }
    }
  }

  // Sortiere Einträge nach Zeitstempel (absteigend)
  std::sort(visibleEntries.begin(), visibleEntries.end(),
    [](const LogEntry& a, const LogEntry& b) { return a.timestamp > b.timestamp; });

  // Begrenze auf count Einträge
  size_t entriesToShow = std::min(count, visibleEntries.size());

  char timestampBuffer[32];
  for (size_t i = 0; i < entriesToShow; ++i) {
    const auto& entry = visibleEntries[i];
    // Rest des Codes bleibt unverändert...
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
    snprintf(buffer, bufferSize, "%lus", millis() / 1000);
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
