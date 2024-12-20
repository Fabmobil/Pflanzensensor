/**
 * @file logger.h
 * @brief Header-Datei für die Logger-Klasse mit Webunterstützung, eingerückter Konsolenausgabe und Datei-Logging
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <vector>
#include <array>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "einstellungen.h"
#include "mutex.h"

extern String logLevel;
extern bool logInDatei;

/**
 * @brief Aufzählung für verschiedene Log-Levels
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Struktur zur Speicherung eines Log-Eintrags
 */
struct LogEintrag {
    LogLevel level;
    String message;
    unsigned long timestamp;
};

// Maximale Anzahl von Log-Einträgen pro Level
const size_t MAX_LOG_EINTRAEGE = 12; // höhere Werte als 12 könen dazu führen, dass der ESP beim Versenden des Webhooks abstürzt!

/**
 * @brief Logger-Klasse zur Handhabung von Log-Nachrichten
 */
class Logger {
public:
    /**
     * @brief Konstruktor für Logger-Klasse
     * @param logLevel Minimales Log-Level zur Anzeige
     * @param useSerial Ob Logs auf Serial ausgegeben werden sollen
     * @param maxEntries Maximale Anzahl von Log-Einträgen im Speicher
     * @param fileLoggingEnabled Ob Datei-Logging aktiviert sein soll
     */
    Logger(LogLevel logLevel = LogLevel::INFO, bool useSerial = true, size_t maxEntries = MAX_LOG_EINTRAEGE, bool fileLoggingEnabled = logInDatei);

    /**
     * @brief Setzt das Log-Level
     * @param level Das neue Log-Level
     */
    void SetzteLogLevel(LogLevel level);

    /**
     * @brief Gibt das aktuelle Log-Level zurück
     * @return Das aktuelle Log-Level
     */
    LogLevel LeseLogLevel() const;

    /**
     * @brief Loggt eine Debug-Nachricht
     * @param message Zu loggende Nachricht
     */
    void debug(const String& message);

    /**
     * @brief Loggt eine Info-Nachricht
     * @param message Zu loggende Nachricht
     */
    void info(const String& message);

    /**
     * @brief Loggt eine Warnungs-Nachricht
     * @param message Zu loggende Nachricht
     */
    void warning(const String& message);

    /**
     * @brief Loggt eine Fehler-Nachricht
     * @param message Zu loggende Nachricht
     */
    void error(const String& message);

    /**
     * @brief Gibt Log-Einträge als formatierte HTML-Tabelle basierend auf aktuellem Log-Level zurück
     * @param count Anzahl der abzurufenden Einträge für jedes Log-Level
     * @return Formatierte HTML-Tabelle der Log-Einträge
     */
    String LogsAlsHtmlTabelle(size_t count = MAX_LOG_EINTRAEGE) const;

    /**
     * @brief Initialisiert NTP-Client zur Zeitabfrage aus dem Internet
     */
    void NTPInitialisieren();

    /**
     * @brief Aktualisiert NTP-Client zur Zeitsynchronisation
     */
    void NTPUpdaten();

    /**
     * @brief Aktiviert oder deaktiviert Datei-Logging
     * @param enable True zum Aktivieren des Datei-Loggings, False zum Deaktivieren
     */
    void LoggenInDatei(bool enable);

    /**
     * @brief Prüft, ob Datei-Logging aktiviert ist
     * @return True, wenn Datei-Logging aktiviert ist, sonst False
     */
    bool IstLoggenInDateiAktiviert() const;

    /**
     * @brief Gibt den Inhalt der Log-Datei zurück
     * @return Inhalt der Log-Datei
     */
    String LogdateiInhaltAuslesen() const;

    /**
     * @brief Löscht die Log-Datei
     */
    void LogdateiLoeschen();

private:
    LogLevel m_logLevel;
    bool m_useSerial;
    size_t m_maxEntries;
    WiFiUDP m_ntpUDP;
    NTPClient* m_timeClient;
    bool m_ntpInitialized;
    bool m_fileLoggingEnabled;
    std::array<std::array<LogEintrag, MAX_LOG_EINTRAEGE>, 4> m_logEntriesByLevel;  // Array von Arrays für jedes Log-Level
    const char* m_logFileName = "/system.log";
    const size_t m_maxFileSize = 100 * 1024;  // 100 KB
    static const size_t PUFFER_GROESSE = 512;
    char m_schreibPuffer[PUFFER_GROESSE];
    size_t m_pufferPosition = 0;
    unsigned long m_letzterFlush = 0;
    mutex_t m_dateiMutex;

    /**
     * @brief Interne Methode zum Loggen einer Nachricht
     * @param level Log-Level der Nachricht
     * @param message Zu loggende Nachricht
     */
    void log(LogLevel level, const String& message);

    /**
     * @brief Gibt Einrückung für Log-Level zurück
     * @param logLevelStr String-Repräsentation des Log-Levels
     * @return Einrückungs-String
     */
    String EinrueckungAuslesen(const char* logLevelStr) const;

    /**
     * @brief Gibt aktuellen Zeitstempel als formatierten String zurück
     * @param buffer Puffer zum Speichern des formatierten Zeitstempels
     * @param bufferSize Größe des Puffers
     */
    void FormatiertenTimestampAusgeben(char* buffer, size_t bufferSize) const;

    /**
     * @brief Schreibt eine Log-Nachricht in die Log-Datei
     * @param logMessage Zu schreibende Nachricht
     */
    void InDateiSchreiben(const char* logMessage);

    void SchreibePufferInDatei();

    /**
     * @brief Kürzt die Log-Datei, wenn sie die maximale Größe überschreitet
     */
    void LogdateiEinkuerzen();

    /**
     * @brief Prüft, ob zu einem bestimmten Datum und Uhrzeit Sommerzeit gilt
     * @param jahr Jahr (vierstellig)
     * @param monat Monat (1-12)
     * @param tag Tag des Monats (1-31)
     * @param stunde Stunde (0-23)
     * @return True, wenn Sommerzeit gilt, sonst False
     */
    static bool istSommerzeit(int jahr, int monat, int tag, int stunde);

    /**
     * @brief Überprüft und bereinigt die Log-Datei bei Bedarf
     */
    void PruefeUndBereinigeDatei();

    /**
     * @brief Prüft, ob genug Speicherplatz für das Logging verfügbar ist
     * @return true wenn genug Speicher verfügbar ist, sonst false
     */
    bool GenugSpeicherVerfuegbar();
};

extern Logger logger;

#endif // LOGGER_H
