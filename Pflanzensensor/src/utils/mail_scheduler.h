/**
 * @file mail_scheduler.h
 * @brief Entscheidet, wann welche Mail fällig ist - ohne Hardware
 * @details Header-only wie utils/chronik_format.h, damit die Entscheidungen
 *          nativ prüfbar sind. Am Gerät ließen sie sich kaum testen: eine
 *          Warnsperre von vier Stunden und ein Lebenszeichen alle 24 Stunden
 *          bräuchten einen Tag pro Versuch.
 *
 *          Gerechnet wird in Epochensekunden, nicht in millis(): die
 *          Abstände überdauern Neustarts, und nach einem Neustart wäre millis()
 *          wieder null - das Gerät würde bei jedem Stromausfall sofort wieder
 *          warnen.
 */

#ifndef MAIL_SCHEDULER_H
#define MAIL_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace Mail {

/// Welche Art Mail ansteht.
enum class Kind : uint8_t {
  None,
  Boot,    ///< einmalig nach dem Start
  Warning, ///< ein überwachter Messwert ist gelb oder rot
  Alive    ///< "bin noch da" mit den aktuellen Messwerten
};

/// Zustand eines Messwerts, wie ihn der Sensor meldet.
enum class Level : uint8_t { Unknown, Green, Yellow, Red };

/**
 * @brief Zustandstext des Sensors in einen Level übersetzen
 * @details Steht hier und nicht beim Versender, weil auch die Vorlagen die
 *          Übersetzung brauchen - und beide müssen dieselbe verwenden.
 */
inline Level levelVonText(const char* status) {
  if (!status) {
    return Level::Unknown;
  }
  if (strcmp(status, "green") == 0) {
    return Level::Green;
  }
  if (strcmp(status, "yellow") == 0) {
    return Level::Yellow;
  }
  if (strcmp(status, "red") == 0) {
    return Level::Red;
  }
  return Level::Unknown;
}

/**
 * @brief Zählt dieser Zustand als auffällig?
 * @details Einzige Quelle der Wahrheit für "auffällig": der Zeitplaner
 *          entscheidet damit über die Warnmail, und der Platzhalter
 *          {auffaellige} füllt damit seine Tabelle. Zwei Regeln wären eine
 *          Warnmail, in der nichts Auffälliges steht.
 */
inline bool istAuffaellig(Level level, Level ab) {
  if (level == Level::Unknown || ab == Level::Unknown) {
    return false;
  }
  return static_cast<uint8_t>(level) >= static_cast<uint8_t>(ab);
}

struct SchedulerConfig {
  bool enabled{false};
  bool bootMail{false};
  bool aliveMail{false};
  /// Mindestabstand zwischen zwei Warnmails. Vorgabe vier Stunden - häufiger
  /// wäre bei einem Messtakt von einer Minute schnell eine Mailflut.
  uint32_t warnIntervalSeconds{4 * 3600};
  uint32_t aliveIntervalSeconds{24 * 3600};
  /// Wartezeit nach dem Start, bevor überhaupt gewarnt wird. Direkt nach dem
  /// Einschalten stehen die Sensoren noch auf "unbekannt" oder wärmen auf; eine
  /// Warnung daraus wäre nur ein Fehlalarm.
  uint32_t settleSeconds{180};
  /// Ab welchem Zustand gewarnt wird. Gelb heißt "Wert wandert aus dem
  /// Wohlfühlbereich", Rot heißt "der Pflanze geht es schlecht". Wer den Sensor
  /// an einer robusten Pflanze hat, will nicht bei jedem gelben Wert eine Mail.
  Level warnFrom{Level::Yellow};
  /// Wie lange die Startmeldung höchstens auf die erste Messung aller Sensoren
  /// wartet. Ohne Obergrenze bliebe sie bei einem defekten Sensor für immer
  /// aus - dann ist eine Mail mit Lücken besser als gar keine.
  uint32_t bootWaitSeconds{300};
};

/**
 * @class Scheduler
 * @brief Führt Buch über gesendete Mails und meldet, was ansteht
 */
class Scheduler {
public:
  void begin(const SchedulerConfig& config, uint32_t now) {
    m_config = config;
    m_startedAt = now;
    m_bootSent = false;
    m_lastWarning = 0;
    m_lastAlive = 0;
    m_warnCount = 0;
    m_sensorsReady = false;
    clearLevels();
  }

  void setConfig(const SchedulerConfig& config) { m_config = config; }
  const SchedulerConfig& config() const { return m_config; }

  /// Zeitpunkte aus dem letzten Lauf übernehmen, damit ein Neustart die
  /// Sperren nicht aufhebt.
  void restore(uint32_t lastWarning, uint32_t lastAlive) {
    m_lastWarning = lastWarning;
    m_lastAlive = lastAlive;
  }

  uint32_t lastWarning() const { return m_lastWarning; }
  /// Zeitpunkt, ab dem die Fristen laufen. 0 heißt: beim Start war keine Uhr da.
  uint32_t startedAt() const { return m_startedAt; }
  /// Startzeitpunkt nachtragen, sobald die Uhr steht.
  void setStartedAt(uint32_t now) { m_startedAt = now; }
  uint32_t lastAlive() const { return m_lastAlive; }

  /**
   * @brief Zustand eines Messwerts melden
   * @param channel Kanalnummer, 0..15
   * @param level Zustand
   * @param watched Wird dieser Kanal überwacht? Nicht überwachte zählen nicht.
   */
  void reportLevel(uint8_t channel, Level level, bool watched) {
    if (channel >= MAX_CHANNELS) {
      return;
    }
    m_levels[channel] = watched ? level : Level::Unknown;
  }

  /// @brief Haben alle eingeschalteten Sensoren einmal gemessen?
  void setSensorsReady(bool ready) { m_sensorsReady = ready; }
  bool sensorsReady() const { return m_sensorsReady; }

  void clearLevels() {
    for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
      m_levels[i] = Level::Unknown;
    }
  }

  /**
   * @brief Ist mindestens ein überwachter Messwert auffällig?
   * @details Auffällig heißt: mindestens so schlimm wie die eingestellte
   *          Schwelle. Level ist aufsteigend nach Dringlichkeit sortiert, aber
   *          Unknown steht davor und darf deshalb nie mitzählen - ein Sensor,
   *          der noch nichts gemeldet hat, ist kein Alarm.
   */
  bool hasAlarm() const {
    for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
      if (istAuffaellig(m_levels[i], m_config.warnFrom)) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Was ist jetzt zu senden?
   * @param now Epochensekunden; 0 heißt "keine Uhr" und verhindert alles
   * @details Reihenfolge: Startmeldung, dann Warnung, dann Lebenszeichen. Eine
   *          Warnung ist dringender als das Lebenszeichen, und beides in einem
   *          Durchlauf zu senden hieße, zweimal hintereinander eine
   *          TLS-Verbindung aufzubauen - dafür ist der Heap zu knapp.
   */
  Kind due(uint32_t now) const {
    if (!m_config.enabled || now < 1600000000UL) {
      return Kind::None;
    }

    if (m_config.bootMail && !m_bootSent) {
      // Erst wenn jeder Sensor einmal gemessen hat. Sonst stünden in der
      // Startmeldung Lücken - beim DHT dauert die erste Messung gut eine
      // Minute, der Versand wäre längst durch.
      if (m_sensorsReady || (now - m_startedAt) >= m_config.bootWaitSeconds) {
        return Kind::Boot;
      }
      return Kind::None;
    }

    if (hasAlarm() && (now - m_startedAt) >= m_config.settleSeconds) {
      // Beim allerersten Mal gibt es keinen Vorgänger - dann sofort senden.
      if (m_lastWarning == 0 || (now - m_lastWarning) >= m_config.warnIntervalSeconds) {
        return Kind::Warning;
      }
    }

    if (m_config.aliveMail) {
      // Nach dem Start läuft die Frist erst an; sonst käme das Lebenszeichen
      // bei jedem Neustart sofort mit.
      const uint32_t bezug = (m_lastAlive == 0) ? m_startedAt : m_lastAlive;
      if ((now - bezug) >= m_config.aliveIntervalSeconds) {
        return Kind::Alive;
      }
    }

    return Kind::None;
  }

  /// @brief Erfolgreich gesendet - Sperren neu setzen
  void markSent(Kind kind, uint32_t now) {
    switch (kind) {
    case Kind::Boot:
      m_bootSent = true;
      break;
    case Kind::Warning:
      m_lastWarning = now;
      m_warnCount++;
      break;
    case Kind::Alive:
      m_lastAlive = now;
      break;
    default:
      break;
    }
  }

  /// @brief Die Startmeldung gilt als erledigt (etwa weil sie abgeschaltet ist)
  void skipBootMail() { m_bootSent = true; }

  uint32_t warningCount() const { return m_warnCount; }

  static constexpr uint8_t MAX_CHANNELS = 16;

private:
  SchedulerConfig m_config;
  Level m_levels[MAX_CHANNELS]{};
  uint32_t m_startedAt{0};
  uint32_t m_lastWarning{0};
  bool m_sensorsReady{false};
  uint32_t m_lastAlive{0};
  uint32_t m_warnCount{0};
  bool m_bootSent{false};
};

/**
 * @brief Steht dieser Kanalschlüssel in der Auswahlliste?
 * @param selection Kommagetrennte Schlüssel, etwa "ANALOG_0,DHT_1"
 * @param key Zu prüfender Schlüssel
 * @details Leere Auswahl heißt "alle" - so verhält sich das Gerät nach dem
 *          Einschalten sinnvoll, ohne dass jemand erst Haken setzen muss.
 *
 *          Verglichen wird mit umschließenden Kommas, sonst träfe "DHT_1" auch
 *          in "DHT_10" - bei einem Sensor mit mehr als zehn Messwerten würde
 *          also stillschweigend der falsche überwacht.
 */
inline bool isWatched(const char* selection, const char* key) {
  if (!selection || !*selection) {
    return true;
  }
  if (!key || !*key) {
    return false;
  }
  const size_t keyLength = strlen(key);
  const char* p = selection;
  while (*p) {
    while (*p == ',' || *p == ' ') {
      p++;
    }
    const char* start = p;
    while (*p && *p != ',') {
      p++;
    }
    size_t length = static_cast<size_t>(p - start);
    while (length > 0 && start[length - 1] == ' ') {
      length--; // Leerzeichen vor dem Komma abschneiden
    }
    if (length == keyLength && strncmp(start, key, keyLength) == 0) {
      return true;
    }
  }
  return false;
}

/// @brief Stunden in Sekunden, mit sinnvollen Grenzen
/// @details Aus der Weboberfläche kommt eine Stundenzahl. Null würde jede
///          Sperre aufheben und das Gerät im Messtakt mailen lassen.
inline uint32_t hoursToSeconds(uint32_t hours, uint32_t minHours = 1, uint32_t maxHours = 720) {
  if (hours < minHours) {
    hours = minHours;
  }
  if (hours > maxHours) {
    hours = maxHours;
  }
  return hours * 3600UL;
}

} // namespace Mail

#endif // MAIL_SCHEDULER_H
