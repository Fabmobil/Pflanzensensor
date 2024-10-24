/**
 * @file Logger.cpp
 * @brief Implementierung der Logger-Klasse mit Webunterstützung, eingerückter Konsolenausgabe und Datei-Logging
 */

#include <LittleFS.h>
#include "logger.h"
#include "einstellungen.h"

Logger logger(LogLevel::DEBUG, true, MAX_LOG_EINTRAEGE, logInDatei);

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

// Beispiel für Log-Nachrichten im Flash-Speicher
const char LOG_MESSAGE_TEMPLATE[] PROGMEM = "%s %*s%s: %s";

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

  // Versuche, das Dateisystem zu initialisieren
  if (!LittleFS.begin()) {
    error(F("Dateisystem konnte nicht initialisiert werden. Versuche zu formatieren..."));
    if (LittleFS.format()) {
      info(F("Dateisystem erfolgreich formatiert. Versuche erneut zu initialisieren..."));
      if (!LittleFS.begin()) {
        error(F("Dateisystem konnte nach Formatierung nicht initialisiert werden."));
        m_fileLoggingEnabled = false; // Deaktiviere Datei-Logging bei Fehler
      } else {
        info(F("Dateisystem erfolgreich initialisiert nach Formatierung."));
      }
    } else {
      error(F("Dateisystem konnte nicht formatiert werden."));
      m_fileLoggingEnabled = false; // Deaktiviere Datei-Logging bei Fehler
    }
  }
  CreateMutex(&m_dateiMutex);
  memset(m_schreibPuffer, 0, PUFFER_GROESSE);
}

void Logger::PruefeUndBereinigeDatei() {
  if (!m_fileLoggingEnabled) return;

  if (LittleFS.exists(F("/system.log"))) {
    File file = LittleFS.open(F("/system.log"), "r");
    if (!file) {
      error(F("Kann Logdatei nicht öffnen. Versuche zu löschen..."));
      LittleFS.remove(F("/system.log"));
    } else {
      // Überprüfe die Dateigröße
      size_t fileSize = file.size();
      file.close();
      if (fileSize > m_maxFileSize) {
        info(F("Logdatei zu groß. Lösche alte Einträge..."));
        LogdateiEinkuerzen();
      }
    }
  } else {
    info(F("Logdatei existiert nicht. Erstelle neue Datei."));
    File file = LittleFS.open(F("/system.log"), "w");
    if (file) {
      file.println(F("Neue Logdatei erstellt"));
      file.close();
    } else {
      error(F("Konnte keine neue Logdatei erstellen"));
    }
  }
}

void Logger::SetzteLogLevel(LogLevel level) {
  m_logLevel = level;
  debug(F("Log-Level gesetzt auf: ") + String(FPSTR(LOG_LEVEL_STRINGS[static_cast<int>(level)])));
}

LogLevel Logger::LeseLogLevel() const {
  return m_logLevel;
}

void Logger::debug(const String& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const String& message) { log(LogLevel::INFO, message); }
void Logger::warning(const String& message) { log(LogLevel::WARNING, message); }
void Logger::error(const String& message) { log(LogLevel::ERROR, message); }

void Logger::LoggenInDatei(bool enable) {
  if (enable && !LittleFS.exists(F("/system.log"))) {
    File file = LittleFS.open(F("/system.log"), "w");
    if (file) {
      file.println(F("Log-Datei erstellt"));
      file.close();
    } else {
      error(F("Log-Datei konnte nicht erstellt werden"));
      return;
    }
  }

  m_fileLoggingEnabled = enable;
  info(enable ? F("Datei-Logging aktiviert") : F("Datei-Logging deaktiviert"));
}

bool Logger::IstLoggenInDateiAktiviert() const {
  return m_fileLoggingEnabled;
}

String Logger::LogdateiInhaltAuslesen() const {
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

void Logger::LogdateiLoeschen() {
  if (!m_fileLoggingEnabled) return;
  File file = LittleFS.open(F("/system.log"), "w");
  if (file) file.close();
}

void Logger::log(LogLevel level, const String& message) {
  if (level < m_logLevel) return;

  char timestampBuffer[32];
  char logMessageBuffer[256];

  FormatiertenTimestampAusgeben(timestampBuffer, sizeof(timestampBuffer));

  LogEintrag entry = {level, message, m_ntpInitialized ? m_timeClient->getEpochTime() : millis()};

  // Füge zum entsprechenden Array basierend auf Log-Level hinzu
  size_t levelIndex = static_cast<size_t>(level);
  if (levelIndex < m_logEntriesByLevel.size()) {
    auto& levelEntries = m_logEntriesByLevel[levelIndex];
    static size_t currentIndex = 0;
    levelEntries[currentIndex] = entry;
    currentIndex = (currentIndex + 1) % MAX_LOG_EINTRAEGE;
  }

  const char* logLevelStr = reinterpret_cast<const char*>(FPSTR(LOG_LEVEL_STRINGS[static_cast<int>(level)]));
  int indent = 5 - strlen(logLevelStr);

  snprintf_P(logMessageBuffer, sizeof(logMessageBuffer), PSTR("%s %*s%s: %s"),
             timestampBuffer, indent, "", logLevelStr, message.c_str());

  if (m_useSerial) {
    Serial.println(logMessageBuffer);
  }

  if (m_fileLoggingEnabled) {
    InDateiSchreiben(logMessageBuffer);
  }
}

void Logger::InDateiSchreiben(const char* logMessage) {
    if (!m_fileLoggingEnabled || !GenugSpeicherVerfuegbar()) return;

    size_t nachrichtLaenge = strlen(logMessage);

    // Mutex für den kritischen Bereich holen
    if (!GetMutex(&m_dateiMutex)) {
        error(F("Konnte Mutex für Dateizugriff nicht erhalten"));
        return;
    }

    // Prüfen ob der Puffer voll wird
    if (m_pufferPosition + nachrichtLaenge + 2 >= PUFFER_GROESSE) {
        SchreibePufferInDatei();
    }

    // Nachricht in Puffer kopieren
    strncpy(m_schreibPuffer + m_pufferPosition, logMessage, PUFFER_GROESSE - m_pufferPosition);
    m_pufferPosition += nachrichtLaenge;
    m_schreibPuffer[m_pufferPosition++] = '\n';

    // Alle 5 Sekunden oder bei vollem Puffer in Datei schreiben
    unsigned long jetztMillis = millis();
    if (jetztMillis - m_letzterFlush >= 5000) {
        SchreibePufferInDatei();
    }

    ReleaseMutex(&m_dateiMutex);
}

void Logger::SchreibePufferInDatei() {
    if (m_pufferPosition == 0) return;

    File file = LittleFS.open(F("/system.log"), "a");
    if (!file) {
        error(F("Konnte Logdatei nicht öffnen"));
        return;
    }

    size_t geschrieben = file.write((uint8_t*)m_schreibPuffer, m_pufferPosition);
    file.close();

    if (geschrieben != m_pufferPosition) {
        error(F("Fehler beim Schreiben in Logdatei"));
    }

    m_pufferPosition = 0;
    m_letzterFlush = millis();

    // Prüfe Dateigröße nur nach erfolgreichem Schreiben
    static unsigned long letzteGroessenPruefung = 0;
    if (millis() - letzteGroessenPruefung >= 60000) { // Maximal einmal pro Minute
        if (file.size() > m_maxFileSize) {
            LogdateiEinkuerzen();
        }
        letzteGroessenPruefung = millis();
    }
}

bool Logger::GenugSpeicherVerfuegbar() {
  FSInfo fs_info;
  LittleFS.info(fs_info);
  return (fs_info.totalBytes - fs_info.usedBytes) > 10240; // Mindestens 10 KB frei
}

void Logger::LogdateiEinkuerzen() {
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

String Logger::LogsAlsHtmlTabelle(size_t count) const {
  String html = F("<table class='log'><tr><th>Zeit</th><th>Level</th><th>Nachricht</th></tr>");

  std::array<LogEintrag, MAX_LOG_EINTRAEGE * 4> allEntries;
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
    [](const LogEintrag& a, const LogEintrag& b) { return a.timestamp > b.timestamp; });

  size_t entriesToShow = std::min(count, totalEntries);

  char timestampBuffer[32];
  for (size_t i = 0; i < entriesToShow; ++i) {
    const auto& entry = allEntries[i];
    FormatiertenTimestampAusgeben(timestampBuffer, sizeof(timestampBuffer));
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

void Logger::FormatiertenTimestampAusgeben(char* buffer, size_t bufferSize) const {
    if (m_ntpInitialized && m_timeClient) {
        // Hole die aktuelle Zeit vom NTP-Client
        time_t epochTime = m_timeClient->getEpochTime();

        // Konvertiere die Epochenzeit in eine struct tm
        struct tm *ptm = gmtime(&epochTime);

        // Prüfe, ob Sommerzeit gilt und passe die Zeit entsprechend an
        int stundenVerschiebung = Logger::istSommerzeit(ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour) ? 2 : 1;

        // Füge die Stundenverschiebung hinzu
        epochTime += stundenVerschiebung * 3600;

        // Aktualisiere die struct tm mit der angepassten Zeit
        ptm = gmtime(&epochTime);

        // Formatiere die Zeit als String im Format "YYYY-MM-DD HH:MM:SS"
        strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", ptm);
    } else {
        // Wenn der NTP-Client nicht initialisiert ist, verwenden wir die Zeit seit Programmstart
        unsigned long t = millis();
        snprintf_P(buffer, bufferSize, PSTR("%lus"), t / 1000);
    }
}

void Logger::NTPInitialisieren() {
    // Zuerst Zeitzone für Mitteleuropa setzen (GMT+1 mit Sommerzeit)
    configTime(1 * 3600, 3600, "pool.ntp.org", "time.nist.gov");

    m_timeClient = new NTPClient(m_ntpUDP, "pool.ntp.org", 0, 60000);
    m_timeClient->begin();
    bool erfolgreich = m_timeClient->forceUpdate(); // Erzwinge erste Synchronisation

    if (erfolgreich) {
        m_ntpInitialized = true;
        debug(F("NTP-Client wurde initialisiert und Zeit synchronisiert"));
    } else {
        warning(F("NTP-Client Initialisierung fehlgeschlagen - erneuter Versuch beim nächsten Update"));
        m_ntpInitialized = false;
    }
}

void Logger::NTPUpdaten() {
    if (m_ntpInitialized && m_timeClient) {
        static unsigned long letzteErfolgreicheAktualisierung = 0;
        static unsigned long letzterAktualisierungsversuch = 0;
        unsigned long aktuelleZeit = millis();

        if (aktuelleZeit - letzterAktualisierungsversuch >= 60000) {
            letzterAktualisierungsversuch = aktuelleZeit;

            if (m_timeClient->update()) {
                if (aktuelleZeit - letzteErfolgreicheAktualisierung >= 600000) {
                    debug(F("NTP-Zeit wurde erfolgreich aktualisiert"));
                }
                letzteErfolgreicheAktualisierung = aktuelleZeit;
            } else {
                if (aktuelleZeit - letzteErfolgreicheAktualisierung >= 3600000) {
                    warning(F("NTP-Zeitaktualisierung fehlgeschlagen"));
                }
            }
        }
    } else {
        static bool nichtInitialisiertGemeldet = false;
        if (!nichtInitialisiertGemeldet) {
            error(F("NTP-Client ist nicht initialisiert"));
            nichtInitialisiertGemeldet = true;
        }
    }
}

bool Logger::istSommerzeit(int jahr, int monat, int tag, int stunde) {
    // Sommerzeit gilt nicht von November bis Februar
    if (monat < 3 || monat > 10) {
        return false;
    }
    // Sommerzeit gilt immer von April bis September
    if (monat > 3 && monat < 10) {
        return true;
    }

    int letzterSonntag = 31 - (5 * jahr / 4 + 4) % 7;

    // Prüfe März (Beginn der Sommerzeit)
    if (monat == 3) {
        return (tag > letzterSonntag || (tag == letzterSonntag && stunde >= 2));
    }
    // Prüfe Oktober (Ende der Sommerzeit)
    else {
        return (tag < letzterSonntag || (tag == letzterSonntag && stunde < 3));
    }
}
