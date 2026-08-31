/**
 * @file chronik_budget.h
 * @brief Wieviel Flash darf die Chronik belegen?
 * @details Header-only und hardwarefrei (siehe chronik_format.h), damit die
 *          Rechnung nativ testbar ist - sie entscheidet, wieviele Segmente
 *          gelöscht werden, und ein Fehler hier kostet entweder Historie oder
 *          den Platz, den Konfiguration und Datei-Log dringend brauchen.
 *
 *          Das Fenster ist bewusst nicht fest verdrahtet, sondern wird aus dem
 *          tatsächlich freien Platz abgeleitet und nach jeder Rotation neu
 *          bestimmt. Wird das Datei-Logging eingeschaltet, schrumpft die
 *          Chronik von selbst.
 */

#ifndef CHRONIK_BUDGET_H
#define CHRONIK_BUDGET_H

#include <stdint.h>

namespace ChronikBudget {

/// Nutzgröße einer Segmentdatei.
///
/// 7936 = 8192 - 256. Der LittleFS-Block ist 8192 B groß, eine Datei von exakt
/// dieser Größe belegt wegen der Blockzeiger im Block aber bereits zwei. Mit
/// 7936 B passt ein Segment garantiert in einen Block. Ein Block je Segment
/// heißt außerdem feinere Rotation (es fliegen rund fünf Stunden auf einmal
/// weg statt zehn) und weniger Verschnitt im angefangenen Segment.
static constexpr uint32_t SEGMENT_SIZE = 7936;

/// Obergrenze, unabhängig vom freien Platz: mehr als 28 Segmente (222 KB)
/// würde auf diesem Gerät nie zusammenkommen, und der Startscan sowie die
/// Segmentliste im ChronikStore sind darauf ausgelegt.
static constexpr uint8_t MAX_SEGMENTS = 28;

/// Spitzenbedarf des Datei-Logs: /log.txt darf 50000 B werden, die Rotation
/// legt zusätzlich /log.txt.tmp mit bis zur halben Größe an (logger.cpp:386ff).
/// Aufgerundet auf ganze Blöcke.
static constexpr uint32_t RESERVE_FILE_LOG_ON = 81920;
/// Auch ohne Datei-Log bleibt Luft: es lässt sich jederzeit einschalten, und
/// bis die nächste Rotation greift, vergehen Minuten.
static constexpr uint32_t RESERVE_FILE_LOG_OFF = 16384;
/// /config/sensor_*.json, /nvs, OTA-Kennzeichen, Konfigurationssicherungen.
static constexpr uint32_t RESERVE_CONFIG = 24576;
/// Zwei Blöcke, die LittleFS zum Kompaktieren der Metadaten braucht. Ohne die
/// scheitern irgendwann auch Löschvorgänge.
static constexpr uint32_t RESERVE_COMPACT = 16384;

struct Input {
  uint32_t freeBytes{0}; ///< LittleFS.info(): totalBytes - usedBytes
  uint32_t ownBytes{0};  ///< was die Chronik selbst bereits belegt
  bool fileLogEnabled{false};
};

/// @brief Wieviel Platz für andere reserviert bleibt
inline uint32_t reserveBytes(bool fileLogEnabled) {
  return (fileLogEnabled ? RESERVE_FILE_LOG_ON : RESERVE_FILE_LOG_OFF) + RESERVE_CONFIG +
         RESERVE_COMPACT;
}

/**
 * @brief Wieviele Segmente darf die Chronik halten?
 * @details Der eigene Verbrauch wird zum freien Platz addiert, weil er beim
 *          Verkleinern wieder frei würde - sonst würde sich das Fenster bei
 *          jeder Prüfung selbst weiter zusammenschrumpfen.
 */
inline uint8_t targetSegments(const Input& in) {
  const uint32_t reserve = reserveBytes(in.fileLogEnabled);
  // Bewusst nicht (frei + eigen - reserve) in einem Rutsch: bei knappem Platz
  // liefe die vorzeichenlose Subtraktion in eine riesige Zahl und die Chronik
  // würde das Dateisystem auffressen.
  const uint32_t verfuegbar = in.freeBytes + in.ownBytes;
  if (verfuegbar <= reserve) {
    return 0;
  }
  const uint32_t nutzbar = verfuegbar - reserve;
  const uint32_t segmente = nutzbar / SEGMENT_SIZE;
  return segmente > MAX_SEGMENTS ? MAX_SEGMENTS : static_cast<uint8_t>(segmente);
}

/// @brief Wieviele der ältesten Segmente müssen weg?
inline uint8_t excessSegments(uint8_t current, uint8_t target) {
  return current > target ? static_cast<uint8_t>(current - target) : 0;
}

} // namespace ChronikBudget

#endif // CHRONIK_BUDGET_H
