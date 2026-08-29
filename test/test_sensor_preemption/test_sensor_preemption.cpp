/**
 * @file test_sensor_preemption.cpp
 * @brief Tests für SensorPreemption::forceImmediateMeasurement()
 *
 * Das ist die Entscheidungslogik hinter dem "Messen"-Button: welcher Sensor
 * bekommt den Slot, und was passiert mit dem bisherigen Halter. Vorher stand
 * diese Logik inline in SensorManager::forceImmediateMeasurement() und war
 * nur über das Gerät (Flashen + Auslösen zweier Sensoren) beobachtbar - genau
 * so wurde sie diese Woche auch tatsächlich geprüft.
 *
 * Getestet wird hier mit zwei echten FakeSensor-Instanzen (siehe
 * test_measurement_cycle/FakeSensor.h) in einem Vektor, wie ihn
 * SensorManager::getSensors() liefert - keine Nachbildung von
 * SensorManager selbst nötig, da forceImmediateMeasurement() als freie
 * Funktion nur den Sensor-Vektor und den Limiter braucht.
 */

#include <unity.h>

#include <Arduino.h>

#include "FakeSensor.h"
#include "managers/manager_sensor_preemption.h"
#include "sensors/sensor_manager_limiter.h"
#include "sensors/sensor_measurement_cycle.h"

namespace {

void releaseAnySlot() {
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();
  const String holder = limiter.getCurrentSensor();
  if (!holder.isEmpty()) {
    limiter.releaseSlot(holder);
  }
}

MeasurementState pumpUntil(SensorMeasurementCycleManager& cycle, MeasurementState target,
                           int maxSteps = 200, unsigned long stepMs = 10) {
  for (int i = 0; i < maxSteps; i++) {
    cycle.updateMeasurementCycle();
    advanceMillis(stepMs);
    if (cycle.getCurrentState() == target) {
      return target;
    }
  }
  return cycle.getCurrentState();
}

} // namespace

/// Unbekannte ID: kein Sensor gefunden, nichts passiert.
void test_unbekannte_id_liefert_false() {
  setMillis(1000);
  releaseAnySlot();

  std::vector<std::unique_ptr<Sensor>> sensors;
  TEST_ASSERT_FALSE(SensorPreemption::forceImmediateMeasurement(sensors, "GEISTERSENSOR"));
}

/// Freier Slot, ein einzelner Sensor: startet ohne Verdrängung.
void test_freier_slot_startet_direkt() {
  setMillis(1000);
  releaseAnySlot();

  std::vector<std::unique_ptr<Sensor>> sensors;
  auto sensor = std::make_unique<FakeSensor>(makeFakeSensorConfig("EINZELN"));
  sensor->setEnabled(true);
  sensor->setCycleManager(std::make_unique<SensorMeasurementCycleManager>(sensor.get()));
  FakeSensor* raw = sensor.get();
  sensors.push_back(std::move(sensor));

  TEST_ASSERT_TRUE(SensorPreemption::forceImmediateMeasurement(sensors, "EINZELN"));
  TEST_ASSERT_TRUE(raw->cycleManager()->isForced());
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("EINZELN"));
}

/**
 * Regressionstest zum eigentlichen Verhalten des "Messen"-Buttons: hält ein
 * ANDERER Sensor den Slot, wird dessen Zyklus abgebrochen (inklusive
 * Deinitialisierung) und der angeforderte Sensor bekommt den Slot sofort -
 * ohne auf die reguläre Slot-Vergabe zu warten.
 */
void test_verdraengt_anderen_haltenden_sensor() {
  setMillis(1000);
  releaseAnySlot();

  std::vector<std::unique_ptr<Sensor>> sensors;

  auto dht = std::make_unique<FakeSensor>(makeFakeSensorConfig("DHT"));
  dht->setEnabled(true);
  dht->setCycleManager(std::make_unique<SensorMeasurementCycleManager>(dht.get()));
  FakeSensor* dhtRaw = dht.get();
  sensors.push_back(std::move(dht));

  auto analog = std::make_unique<FakeSensor>(makeFakeSensorConfig("ANALOG"));
  analog->setEnabled(true);
  analog->setCycleManager(std::make_unique<SensorMeasurementCycleManager>(analog.get()));
  FakeSensor* analogRaw = analog.get();
  sensors.push_back(std::move(analog));

  // DHT fängt zuerst an zu messen und hält den Slot.
  pumpUntil(*dhtRaw->cycleManager(), MeasurementState::MEASURING, 50, 10);
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("DHT"));
  TEST_ASSERT_GREATER_THAN(0, dhtRaw->initCallCount);

  // Jetzt wird ANALOG manuell ausgelöst - wie ein Klick auf "Messen".
  TEST_ASSERT_TRUE(SensorPreemption::forceImmediateMeasurement(sensors, "ANALOG"));

  // DHT wurde abgebrochen und sauber deinitialisiert, nicht nur unterbrochen.
  TEST_ASSERT_TRUE(dhtRaw->cycleManager()->getCurrentState() == MeasurementState::WAITING_FOR_DUE);
  TEST_ASSERT_GREATER_THAN(0, dhtRaw->deinitCallCount);

  // ANALOG hält jetzt sofort den Slot - kein Warten auf die reguläre Vergabe.
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("ANALOG"));
  TEST_ASSERT_TRUE(analogRaw->cycleManager()->isForced());

  // Der verdrängte Sensor kommt danach ganz normal wieder zum Zug.
  MeasurementState reached =
      pumpUntil(*analogRaw->cycleManager(), MeasurementState::PROCESSING, 100, 5);
  TEST_ASSERT_TRUE(reached == MeasurementState::PROCESSING);
}

/// Der Slot war von einer ID belegt, zu der es keinen Sensor mehr gibt
/// (entfernt/deaktiviert) - der Slot muss trotzdem freigegeben werden, sonst
/// bliebe er bis zum Timeout blockiert.
void test_gibt_slot_verwaisten_halters_frei() {
  setMillis(1000);
  releaseAnySlot();

  SensorManagerLimiter::getInstance().forceTakeSlot("VERSCHWUNDEN");

  std::vector<std::unique_ptr<Sensor>> sensors;
  auto sensor = std::make_unique<FakeSensor>(makeFakeSensorConfig("NEU"));
  sensor->setEnabled(true);
  sensor->setCycleManager(std::make_unique<SensorMeasurementCycleManager>(sensor.get()));
  sensors.push_back(std::move(sensor));

  TEST_ASSERT_TRUE(SensorPreemption::forceImmediateMeasurement(sensors, "NEU"));
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("NEU"));
}

/**
 * Fordert ein Sensor seinen EIGENEN Slot erneut an (er hält ihn schon,
 * mitten in MEASURING) - z.B. ein zweiter Klick auf "Messen" während die
 * Messung noch läuft. forceImmediateMeasurement() bricht dabei IMMER den
 * eigenen Zyklus ab und startet neu, das ist gewollt (ein erneuter Klick
 * soll die Messung neu beginnen, nicht auf die alte warten).
 *
 * Was NICHT passieren darf: dass der "anderer Sensor hält den Slot"-Zweig
 * (holder != id) fälschlich zusätzlich greift, weil holder==id nicht sauber
 * erkannt wird - das würde den Zyklus ein zweites Mal abbrechen und den Slot
 * unnötig über releaseSlot()/forceTakeSlot() doppelt anfassen. Genau ein
 * Abbruch (aus dem eigenen forceImmediateMeasurement()), nicht zwei.
 */
void test_eigener_slot_wird_nicht_zusaetzlich_als_fremd_abgebrochen() {
  setMillis(1000);
  releaseAnySlot();

  std::vector<std::unique_ptr<Sensor>> sensors;
  auto sensor = std::make_unique<FakeSensor>(makeFakeSensorConfig("SELBST"));
  sensor->setEnabled(true);
  sensor->setCycleManager(std::make_unique<SensorMeasurementCycleManager>(sensor.get()));
  FakeSensor* raw = sensor.get();
  sensors.push_back(std::move(sensor));

  pumpUntil(*raw->cycleManager(), MeasurementState::MEASURING, 50, 10);
  int deinitVorher = raw->deinitCallCount;

  TEST_ASSERT_TRUE(SensorPreemption::forceImmediateMeasurement(sensors, "SELBST"));

  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("SELBST"));
  TEST_ASSERT_EQUAL_INT(deinitVorher + 1, raw->deinitCallCount);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_unbekannte_id_liefert_false);
  RUN_TEST(test_freier_slot_startet_direkt);
  RUN_TEST(test_verdraengt_anderen_haltenden_sensor);
  RUN_TEST(test_gibt_slot_verwaisten_halters_frei);
  RUN_TEST(test_eigener_slot_wird_nicht_zusaetzlich_als_fremd_abgebrochen);
  return UNITY_END();
}
