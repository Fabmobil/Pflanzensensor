/**
 * @file pending_update_queue.h
 * @brief Reine Warteschlangen-Logik der Write-Behind-Persistenz
 * @details Eigene, hardwareunabhängige Datei aus demselben Grund wie
 *          utils/crc32.h und managers/manager_sensor_preemption.h: die
 *          Entscheidung "ist das ein Duplikat, muss der älteste Eintrag
 *          weichen" hängt an nichts Hardwarespezifischem. Was tatsächlich
 *          mit einem verdrängten Eintrag passiert (auf Flash schreiben),
 *          bleibt bewusst außerhalb - das ist Sache von
 *          manager_sensor_persistence.cpp, das dafür LittleFS/Preferences
 *          braucht.
 *
 *          PendingUpdate/PendingUpdateType standen vorher datei-lokal in
 *          manager_sensor_persistence.cpp; hierher verschoben, damit die
 *          Warteschlangenlogik sie ohne den Rest der Datei benutzen kann.
 */

#ifndef PENDING_UPDATE_QUEUE_H
#define PENDING_UPDATE_QUEUE_H

#include <Arduino.h>

#include <optional>
#include <vector>

enum class PendingUpdateType {
  RAW_MIN_MAX,        // int absoluteRawMin, absoluteRawMax
  ABSOLUTE_MIN_MAX,   // float absoluteMin, absoluteMax
  CALIBRATED_MIN_MAX, // int minValue, maxValue, bool inverted
  LAST_VALUE          // float lastValue, int lastRawValue
};

struct PendingUpdate {
  PendingUpdateType type;
  String sensorId;
  size_t measurementIndex;
  unsigned long timestamp; // When this update was queued

  // Union to save memory - only one set of values is active at a time
  union {
    struct {
      int absoluteRawMin;
      int absoluteRawMax;
    } raw;
    struct {
      float absoluteMin;
      float absoluteMax;
    } absolute;
    struct {
      int minValue;
      int maxValue;
      bool inverted;
    } calibrated;
    struct {
      float lastValue;
      int lastRawValue;
    } last;
  } data;
};

namespace PendingUpdateQueue {

/**
 * @brief Reiht ein Update ein oder aktualisiert einen vorhandenen Eintrag
 *        gleichen Typs/Sensors/Index
 * @param queue Die Warteschlange
 * @param update Das neue Update
 * @param maxSize Obergrenze der Warteschlange
 * @return Der verdrängte (älteste) Eintrag, falls die Warteschlange voll war
 *         und deshalb Platz gemacht werden musste - sonst leer. Der Aufrufer
 *         ist dafür verantwortlich, einen zurückgegebenen Eintrag auf Flash
 *         zu schreiben; diese Funktion selbst tut keine I/O.
 * @details Ein vorhandener Eintrag mit gleichem (type, sensorId,
 *          measurementIndex) wird überschrieben statt einen zweiten
 *          anzulegen - ohne das würde z.B. jede Änderung des Messwerts
 *          eines Sensors einen eigenen Warteschlangenplatz belegen, statt
 *          den vorherigen zu ersetzen.
 */
inline std::optional<PendingUpdate> enqueue(std::vector<PendingUpdate>& queue, PendingUpdate update,
                                            size_t maxSize) {
  for (auto& existing : queue) {
    if (existing.type == update.type && existing.sensorId == update.sensorId &&
        existing.measurementIndex == update.measurementIndex) {
      existing.data = update.data;
      existing.timestamp = update.timestamp;
      return std::nullopt;
    }
  }

  std::optional<PendingUpdate> evicted;
  if (queue.size() >= maxSize) {
    evicted = queue.front();
    queue.erase(queue.begin());
  }

  queue.push_back(std::move(update));
  return evicted;
}

} // namespace PendingUpdateQueue

#endif // PENDING_UPDATE_QUEUE_H
