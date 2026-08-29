/**
 * @file manager_sensor_preemption.h
 * @brief Entscheidungslogik hinter dem "Messen"-Button: wer wird verdrängt?
 * @details Eigene, hardwareunabhängige Datei statt Methode direkt auf
 *          SensorManager, aus demselben Grund wie utils/crc32.h: die
 *          Entscheidung "welcher Sensor hält gerade den Messslot und muss
 *          verdrängt werden" hängt an nichts Hardwarespezifischem und ist
 *          damit ohne Preferences/LittleFS/Sensorbibliotheken testbar -
 *          SensorManager selbst braucht all das über SensorFactory.
 */

#ifndef MANAGER_SENSOR_PREEMPTION_H
#define MANAGER_SENSOR_PREEMPTION_H

#include <algorithm>
#include <memory>
#include <vector>

#include "logger/logger.h"
#include "sensors/sensor_manager_limiter.h"
#include "sensors/sensor_measurement_cycle.h"
#include "sensors/sensors.h"

namespace SensorPreemption {

/**
 * @brief Löst für den Sensor mit der angegebenen ID sofort eine Messung aus,
 *        notfalls unter Verdrängung des aktuellen Slot-Halters
 * @param sensors Alle verwalteten Sensoren (SensorManager::getSensors())
 * @param id ID des Sensors, für den sofort gemessen werden soll
 * @return true wenn der Sensor gefunden wurde und die Messung ausgelöst
 *         werden konnte, false wenn kein Sensor mit dieser ID existiert oder
 *         er keinen Zyklusmanager hat
 * @details Hält ein anderer Sensor den Messslot, wird dessen Zyklus
 *          abgebrochen (abortCycle()) statt zu warten - das könnte je nach
 *          Sensor zehn Sekunden und mehr dauern. Existiert der Halter nicht
 *          mehr (entfernt/deaktiviert), wird der Slot direkt freigegeben,
 *          damit er nicht bis zum Timeout blockiert bleibt.
 */
inline bool forceImmediateMeasurement(const std::vector<std::unique_ptr<Sensor>>& sensors,
                                      const String& id) {
  auto find = [&sensors](const String& sensorId) -> Sensor* {
    auto it = std::find_if(sensors.begin(), sensors.end(),
                           [&sensorId](const auto& s) { return s && s->getId() == sensorId; });
    return it != sensors.end() ? it->get() : nullptr;
  };

  Sensor* sensor = find(id);
  if (!sensor || !sensor->cycleManager()) {
    return false;
  }

  auto& limiter = SensorManagerLimiter::getInstance();
  // Kopie, nicht Referenz: abortCycle()/releaseSlot() leeren das Member.
  const String holder = limiter.getCurrentSensor();

  if (!holder.isEmpty() && holder != id) {
    // Es misst gerade ein anderer Sensor. Warten würde je nach Sensor zehn
    // Sekunden und mehr dauern, deshalb wird dessen Zyklus abgebrochen.
    Sensor* other = find(holder);
    if (other && other->cycleManager()) {
      LOG_INFO(F("SensorManager"), String(F("Messung von ")) + holder +
                                       String(F(" wird für die manuell ausgelöste Messung von ")) +
                                       id + F(" abgebrochen"));
      other->cycleManager()->abortCycle();
    } else {
      // Halter existiert nicht mehr (Sensor entfernt/deaktiviert) - Slot
      // wäre sonst bis zum Timeout blockiert.
      LOG_WARN(F("SensorManager"),
               String(F("Messslot war von unbekanntem Sensor ")) + holder + F(" belegt"));
      limiter.releaseSlot(holder);
    }
  }

  sensor->cycleManager()->forceImmediateMeasurement();
  // Slot direkt zuteilen, damit die Messung im selben Schleifendurchlauf
  // beginnen kann statt erst beim nächsten Slot-Versuch.
  limiter.forceTakeSlot(id);
  return true;
}

} // namespace SensorPreemption

#endif // MANAGER_SENSOR_PREEMPTION_H
