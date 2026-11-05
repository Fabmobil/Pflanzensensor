/**
 * @file budget.h
 * @brief Wieviel Flash darf die Chronik belegen? (aus dem Altcode)
 * @details Das Fenster wird aus dem freien Platz abgeleitet und nach jeder
 *          Rotation neu bestimmt.
 */
#pragma once

#include <stdint.h>

namespace ps {
namespace chronik {

/// 8192 − 256: passt garantiert in einen LittleFS-Block.
constexpr uint32_t kSegmentSize = 7936;
constexpr uint8_t kMaxSegments = 28;
/// Belegung eines Segments im Flash: ein 8-KB-Block (ESP8266) bzw. zwei
/// 4-KB-Blöcke (ESP32), unabhängig vom Füllstand. Für ownBytes verwenden,
/// sonst zählt ein frisch begonnenes Segment fast nichts, belegt aber schon
/// einen ganzen Block - und das Budget schrumpft bei jeder Rotation.
constexpr uint32_t kSegmentFootprint = 8192;

/// Datei-Log: 16000 B plus Rotationskopie bis zur Hälfte → drei Blöcke.
constexpr uint32_t kMaxLogFileSize = 16000;
constexpr uint32_t kReserveFileLogOn = 24576;
constexpr uint32_t kReserveFileLogOff = 16384;
/// Konfiguration ohne Unterverzeichnisse (refactor.md 5.7): device.cfg,
/// sensors.cfg, secrets.cfg, state.bin, mail.tpl je höchstens ein Block, dazu
/// eine .tmp-Datei beim atomaren Schreiben und /import.tmp → 7 Blöcke.
constexpr uint32_t kReserveConfig = 7 * 8192;
/// Zwei Blöcke für die Kompaktierung der LittleFS-Metadaten.
constexpr uint32_t kReserveCompact = 16384;

static_assert(kReserveFileLogOn >= ((kMaxLogFileSize + kMaxLogFileSize / 2 + 8191) / 8192) * 8192,
              "Reserve deckt die Rotationsspitze des Datei-Logs nicht ab");

struct BudgetInput {
  uint32_t freeBytes = 0;
  uint32_t ownBytes = 0;
  bool fileLogEnabled = false;
};

inline uint32_t reserveBytes(bool fileLog) {
  return (fileLog ? kReserveFileLogOn : kReserveFileLogOff) + kReserveConfig + kReserveCompact;
}

inline uint8_t targetSegments(const BudgetInput& in) {
  const uint32_t reserve = reserveBytes(in.fileLogEnabled);
  const uint32_t avail = in.freeBytes + in.ownBytes;
  if (avail <= reserve)
    return 0;
  const uint32_t segs = (avail - reserve) / kSegmentSize;
  return segs > kMaxSegments ? kMaxSegments : static_cast<uint8_t>(segs);
}

inline uint8_t excessSegments(uint8_t current, uint8_t target) {
  return current > target ? static_cast<uint8_t>(current - target) : 0;
}

} // namespace chronik
} // namespace ps
