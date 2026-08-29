/**
 * @file test_measurement_cycle.cpp
 * @brief Tests für den vollständigen SensorMeasurementCycleManager
 *
 * Anders als test_limiter und test_messzustand, die einzelne Bausteine
 * isoliert prüfen, läuft hier die komplette Zustandsmaschine gegen einen
 * echten SensorMeasurementCycleManager und einen FakeSensor, der die
 * unveränderte Basisklasse Sensor benutzt (siehe FakeSensor.h). Getestet wird
 * damit dasselbe Verhalten, das zuvor nur am Gerät durch Flashen und
 * Mitlesen des seriellen Logs beobachtbar war - unter anderem die beiden
 * Szenarien, die diese Woche als echte Fehler auffielen:
 *
 *   - Slot-Selbstblockade bei forceImmediateMeasurement() mitten im Zyklus
 *   - hängender Sensor in MEASURING, der den Slot ohne Zeitschranke ewig
 *     gehalten hätte
 *
 * sowie die neu eingeführte Rückfallstufe (Backoff statt Abschaltung).
 */

#include <unity.h>

#include <Arduino.h>

#include "FakeSensor.h"
#include "sensors/sensor_manager_limiter.h"
#include "sensors/sensor_measurement_cycle.h"

namespace {

/// Diese Konstanten sind privat in SensorMeasurementCycleManager - hier
/// dupliziert, um Zeit gezielt vorzuspulen. Kommentar an der Kopie hält sie
/// an der Quelle fest, damit ein künftiger Wert-Wechsel nicht unbemerkt
/// auseinanderläuft.
namespace CycleTimings { // siehe sensors/sensor_measurement_cycle.h
constexpr unsigned long INIT_DELAY = 100;
constexpr unsigned long WARMUP_DELAY = 100;
constexpr unsigned long SLOT_RETRY_DELAY = 50;
constexpr unsigned long MEASURE_TIMEOUT = 30000;
constexpr unsigned long ERROR_RETRY_DELAY = 1000;
} // namespace CycleTimings

void releaseAnySlot() {
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();
  const String holder = limiter.getCurrentSensor();
  if (!holder.isEmpty()) {
    limiter.releaseSlot(holder);
  }
}

/// Treibt die Zustandsmaschine an, bis targetState erreicht ist oder
/// maxSteps überschritten wird. Jeder Schritt spult die Uhr um stepMs
/// weiter - klein genug, um interne Verzögerungen (SLOT_RETRY_DELAY etc.)
/// sauber zu treffen.
MeasurementState pumpUntil(SensorMeasurementCycleManager& cycle, MeasurementState target,
                           int maxSteps = 200, unsigned long stepMs = 10) {
  // Absichtlich ERST schalten, DANN prüfen: WAITING_FOR_DUE ist zugleich der
  // Ruhezustand. Prüfte man zuerst, kehrte der Aufruf für dieses Ziel sofort
  // zurück, ohne dass überhaupt ein Zyklus gelaufen wäre.
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

// ---------------------------------------------------------------- Normalfall

/// Ein ungestörter Zyklus durchläuft alle Zustände und liefert den erwarteten
/// Messwert - Grundlage, an der sich alle folgenden Abweichungstests messen.
void test_kompletter_zyklus_liefert_messwert() {
  setMillis(1000);
  releaseAnySlot();

  SensorConfig config = makeFakeSensorConfig("HAPPY");
  FakeSensor sensor(config);
  sensor.setEnabled(true);
  sensor.sampleValue = 23.5f;
  // deinitialize() markiert die Messdaten als ungültig (getMeasurementData()
  // liefert danach die statische invalid-Instanz mit lauter Nullen) - für die
  // Wertkontrolle muss der Sensor initialisiert bleiben.
  sensor.deinitAfterMeasurement = false;

  SensorMeasurementCycleManager cycle(&sensor);

  MeasurementState reached = pumpUntil(cycle, MeasurementState::WAITING_FOR_DUE, 500, 20);
  TEST_ASSERT_TRUE(reached == MeasurementState::WAITING_FOR_DUE);
  TEST_ASSERT_EQUAL_FLOAT(23.5f, sensor.getMeasurementData().values[0]);

  // Der Slot muss nach DEINITIALIZING wieder frei sein - sonst bliebe jeder
  // weitere Sensor blockiert.
  TEST_ASSERT_FALSE(SensorManagerLimiter::getInstance().hasSlot("HAPPY"));
}

// --------------------------------------------- Erzwungene Messung / Abbruch

/**
 * Regressionstest zum 42-Sekunden-Fehler: forceImmediateMeasurement() mitten
 * im laufenden Zyklus darf den Sensor nicht in seinem eigenen Messslot
 * einsperren. Vorher gab acquireSlot() für den bereits haltenden Sensor
 * false zurück, und der Sensor wartete bis zur Zwangsfreigabe des Limiters
 * (45 s).
 */
void test_forceImmediateMeasurement_bricht_laufenden_zyklus_ab() {
  setMillis(1000);
  releaseAnySlot();

  SensorConfig config = makeFakeSensorConfig("FORCED");
  FakeSensor sensor(config);
  sensor.setEnabled(true);

  SensorMeasurementCycleManager cycle(&sensor);

  // Mitten im Zyklus anhalten - nach der Initialisierung, noch vor dem Messwert.
  MeasurementState reached = pumpUntil(cycle, MeasurementState::MEASURING, 50, 10);
  TEST_ASSERT_TRUE(reached == MeasurementState::MEASURING);
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("FORCED"));

  cycle.forceImmediateMeasurement();

  // abortCycle() muss sofort - ohne weiteren updateMeasurementCycle()-Aufruf -
  // in WAITING_FOR_DUE zurückfallen und den Slot freigeben.
  TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::WAITING_FOR_DUE);
  TEST_ASSERT_FALSE(SensorManagerLimiter::getInstance().hasSlot("FORCED"));
  TEST_ASSERT_TRUE(cycle.isDue());

  // Der neue, erzwungene Zyklus darf nicht erneut an der Slot-Vergabe
  // scheitern (das war exakt der Fehler) und muss ohne INIT_DELAY durchlaufen.
  reached = pumpUntil(cycle, MeasurementState::PROCESSING, 50, 5);
  TEST_ASSERT_TRUE(reached == MeasurementState::PROCESSING);
}

/// Ohne Verdrängung müsste ein zweiter Sensor auf den Slot warten. abortCycle()
/// ist der Baustein, den SensorManager::forceImmediateMeasurement() nutzt, um
/// den Slot eines FREMDEN Sensors freizugeben, bevor er ihn dem angeforderten
/// zuteilt (forceTakeSlot()) - hier isoliert am Zyklus des verdrängten
/// Sensors geprüft.
void test_abortCycle_gibt_fremd_gehaltenen_slot_frei() {
  setMillis(1000);
  releaseAnySlot();

  SensorConfig config = makeFakeSensorConfig("VERDRAENGT");
  FakeSensor sensor(config);
  sensor.setEnabled(true);

  SensorMeasurementCycleManager cycle(&sensor);
  pumpUntil(cycle, MeasurementState::MEASURING, 50, 10);
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("VERDRAENGT"));

  // So ruft SensorManager::forceImmediateMeasurement() den Abbruch des
  // bisherigen Halters auf.
  cycle.abortCycle();

  TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::WAITING_FOR_DUE);
  TEST_ASSERT_FALSE(SensorManagerLimiter::getInstance().hasSlot("VERDRAENGT"));
  // Der verdrängte Sensor wurde für diesen Zyklus initialisiert und muss
  // sauber deinitialisiert worden sein, sonst bliebe z.B. ein Multiplexer auf
  // einem Kanal stehen.
  TEST_ASSERT_GREATER_THAN(0, sensor.deinitCallCount);
}

// ------------------------------------------------------- Zeitschranken (A1)

/**
 * Regressionstest für die fehlende MEASURE_TIMEOUT-Prüfung: ein Sensor, der
 * in MEASURING dauerhaft PENDING liefert, muss nach der Zeitschranke selbst
 * aufgeben - nicht erst nach der Zwangsfreigabe des Limiters (45 s), die den
 * Halter nicht einmal benachrichtigt. Vorher konnte in der Zwischenzeit ein
 * zweiter Sensor denselben Slot bekommen und parallel messen.
 */
void test_haengender_sensor_wird_nach_zeitschranke_abgebrochen() {
  setMillis(1000);
  releaseAnySlot();

  SensorConfig config = makeFakeSensorConfig("HAENGT");
  FakeSensor sensor(config);
  sensor.setEnabled(true);
  sensor.hangInMeasuring = true;

  SensorMeasurementCycleManager cycle(&sensor);
  MeasurementState reached = pumpUntil(cycle, MeasurementState::MEASURING, 50, 10);
  TEST_ASSERT_TRUE(reached == MeasurementState::MEASURING);
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("HAENGT"));

  // Kurz vor der Zeitschranke: noch nichts passiert.
  advanceMillis(CycleTimings::MEASURE_TIMEOUT - 100);
  cycle.updateMeasurementCycle();
  TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::MEASURING);

  // Zeitschranke überschritten: die Zustandsmaschine muss selbst abbrechen -
  // deutlich vor SensorManagerLimiter::SLOT_TIMEOUT_MS (45 s).
  advanceMillis(200);
  cycle.updateMeasurementCycle();

  TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::ERROR);
  TEST_ASSERT_FALSE(SensorManagerLimiter::getInstance().hasSlot("HAENGT"));
  TEST_ASSERT_TRUE(cycle.getLastError().length() > 0);
}

/// Slot-Invariante: hält die Zustandsmaschine einen Zustand, der den Slot
/// voraussetzt, der Limiter aber sagt "nicht mehr belegt" (z.B. weil
/// SensorManager ihn einem anderen Sensor zugeteilt hat), muss der Zyklus
/// abbrechen statt weiterzumessen. Ohne diese Prüfung hätten zwei Sensoren
/// gleichzeitig auf gemeinsamer Hardware gemessen.
void test_slot_invariante_bricht_bei_slotverlust_ab() {
  setMillis(1000);
  releaseAnySlot();

  SensorConfig config = makeFakeSensorConfig("SLOTVERLUST");
  FakeSensor sensor(config);
  sensor.setEnabled(true);

  SensorMeasurementCycleManager cycle(&sensor);
  pumpUntil(cycle, MeasurementState::MEASURING, 50, 10);
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("SLOTVERLUST"));

  // Der Slot wird - ohne Wissen dieses Zyklusmanagers - einem anderen Sensor
  // zugeteilt (das tut SensorManager::forceImmediateMeasurement() für eine
  // manuell ausgelöste Messung eines anderen Sensors).
  SensorManagerLimiter::getInstance().forceTakeSlot("ANDERER");

  cycle.updateMeasurementCycle();

  TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::WAITING_FOR_DUE);
  TEST_ASSERT_TRUE(SensorManagerLimiter::getInstance().hasSlot("ANDERER"));

  releaseAnySlot();
}

// --------------------------------------------------- Fehlzählung / Backoff

/**
 * Regressionstest für A2 (Fehlzähler) und A3 (Backoff statt Abschaltung):
 * ein Sensor, der MEASUREMENT_ERROR_COUNT (5) mal in Folge scheitert und
 * dessen Reinitialisierungsversuch danach ebenfalls fehlschlägt, muss
 *   - aktiviert bleiben (kein setEnabled(false) mehr),
 *   - hasPersistentError setzen, damit der Zustand im Webinterface sichtbar
 *     ist,
 * und sich nach der ersten wieder erfolgreichen Messung selbst heilen
 * (hasPersistentError wieder false, Fehlerzähler zurückgesetzt).
 *
 * initFailAfterCallNumber=5 lässt genau den Reinitialisierungsversuch aus
 * handleError() scheitern: fünf reguläre Zyklen rufen init() je einmal auf
 * (Aufrufe 1-5, alle erfolgreich), der sechste Aufruf ist der gezielte
 * Reinitialisierungsversuch bei erschöpften Wiederholungen und schlägt fehl.
 */
void test_fehlerzaehler_fuehrt_zu_backoff_nicht_zu_abschaltung() {
  setMillis(1000);
  releaseAnySlot();

  SensorConfig config = makeFakeSensorConfig("FLAPPY");
  // Schnelle Wiederholung nötig: fünf fehlschlagende Zyklen hintereinander
  // sollen in simulierten Millisekunden laufen, nicht über fünf Minuten
  // realer Intervalle.
  config.measurementInterval = 0;
  FakeSensor sensor(config);
  sensor.setEnabled(true);
  sensor.measurementsFail = true;
  sensor.initFailAfterCallNumber = 5;

  SensorMeasurementCycleManager cycle(&sensor);

  // Fünf vollständige, scheiternde Zyklen durchlaufen: WAITING_FOR_DUE ->
  // ... -> MEASURING (scheitert intern nach MAX_RETRIES) -> ERROR ->
  // handleError() -> zurück zu WAITING_FOR_DUE (solange errorCount < 5).
  for (int cycleNum = 0; cycleNum < 5; cycleNum++) {
    pumpUntil(cycle, MeasurementState::ERROR, 100, 10);
    TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::ERROR);
    // handleError() braucht ERROR_RETRY_DELAY, bevor es zurück auf
    // WAITING_FOR_DUE (bzw. beim letzten Mal in den Backoff) wechselt.
    advanceMillis(CycleTimings::ERROR_RETRY_DELAY + 50);
    cycle.updateMeasurementCycle();
  }

  // Nach fünf aufeinanderfolgenden Fehlern und gescheiterter
  // Reinitialisierung: aktiv, aber als dauerhaft fehlerhaft markiert.
  TEST_ASSERT_TRUE(sensor.isEnabled());
  TEST_ASSERT_TRUE(sensor.config().hasPersistentError);
  TEST_ASSERT_TRUE(cycle.getCurrentState() == MeasurementState::WAITING_FOR_DUE);

  // Selbstheilung: der Sensor liefert wieder gültige Werte.
  sensor.measurementsFail = false;
  sensor.initFailAfterCallNumber = -1;

  // Über die erste Backoff-Stufe (1000 ms, siehe scheduleRetryWithBackoff())
  // hinaus vorspulen. Da das Intervall weiterhin 0 ist, läuft die
  // Zustandsmaschine danach ununterbrochen von Zyklus zu Zyklus - ein fester
  // Endzustand lässt sich deshalb nicht sinnvoll vorhersagen. Geprüft wird
  // stattdessen die eigentliche Aussage: irgendwann innerhalb der ersten
  // erneut versuchten Zyklen räumt eine erfolgreiche Messung
  // hasPersistentError wieder ab (siehe handleMeasuring()-Erfolgspfad).
  advanceMillis(1500);
  bool healed = false;
  for (int i = 0; i < 60 && !healed; i++) {
    cycle.updateMeasurementCycle();
    advanceMillis(20);
    healed = !sensor.config().hasPersistentError;
  }

  TEST_ASSERT_TRUE(healed);
  TEST_ASSERT_TRUE(sensor.isEnabled());
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_kompletter_zyklus_liefert_messwert);
  RUN_TEST(test_forceImmediateMeasurement_bricht_laufenden_zyklus_ab);
  RUN_TEST(test_abortCycle_gibt_fremd_gehaltenen_slot_frei);
  RUN_TEST(test_haengender_sensor_wird_nach_zeitschranke_abgebrochen);
  RUN_TEST(test_slot_invariante_bricht_bei_slotverlust_ab);
  RUN_TEST(test_fehlerzaehler_fuehrt_zu_backoff_nicht_zu_abschaltung);
  return UNITY_END();
}
