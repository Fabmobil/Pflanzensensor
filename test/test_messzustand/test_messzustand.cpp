/**
 * @file test_messzustand.cpp
 * @brief Tests für MeasurementStateInfo (Zustandsverwaltung des Messzyklus)
 *
 * Deckt die Fälligkeitsrechnung, die Mindestverzögerung und die Fehlerzählung
 * ab. Letztere ist der Regressionsschutz für den Fehler, bei dem errorCount
 * als "aufeinanderfolgende Fehler" dokumentiert war, in Wahrheit aber
 * kumulierte, weil ihn keine erfolgreiche Messung zurücksetzte.
 */

#include <unity.h>

#include <Arduino.h>

#include "sensors/sensor_measurement_state.h"

/// Frisch angelegt ist noch nichts fällig, solange nextDueTime in der Zukunft liegt.
void test_nicht_faellig_vor_dem_zeitpunkt() {
  setMillis(1000);
  MeasurementStateInfo state;
  state.scheduleNextMeasurement(millis(), 5000);

  TEST_ASSERT_FALSE(state.isDue());
  advanceMillis(4999);
  TEST_ASSERT_FALSE(state.isDue());
}

/// Ab dem geplanten Zeitpunkt ist die Messung fällig.
void test_faellig_ab_dem_zeitpunkt() {
  setMillis(1000);
  MeasurementStateInfo state;
  state.scheduleNextMeasurement(millis(), 5000);

  advanceMillis(5000);
  TEST_ASSERT_TRUE(state.isDue());
}

/// Ein Intervall von 0 macht sofort fällig - der Weg der manuell ausgelösten Messung.
void test_intervall_null_ist_sofort_faellig() {
  setMillis(50000);
  MeasurementStateInfo state;
  state.scheduleNextMeasurement(millis(), 0);

  TEST_ASSERT_TRUE(state.isDue());
}

/// Die Mindestverzögerung läuft ab und gibt dann frei.
void test_mindestverzoegerung() {
  setMillis(1000);
  MeasurementStateInfo state;
  state.setMinimumDelay(100);

  TEST_ASSERT_FALSE(state.isMinimumDelayElapsed());
  advanceMillis(100);
  TEST_ASSERT_TRUE(state.isMinimumDelayElapsed());
}

/// Eine Verzögerung von 0 ist sofort abgelaufen (erzwungene Messung).
void test_mindestverzoegerung_null() {
  setMillis(1000);
  MeasurementStateInfo state;
  state.setMinimumDelay(0);

  TEST_ASSERT_TRUE(state.isMinimumDelayElapsed());
}

/// recordError() zählt hoch und meldet ab MEASUREMENT_ERROR_COUNT einen fatalen Fehler.
void test_fehlerzaehler_erreicht_grenze() {
  setMillis(1000);
  MeasurementStateInfo state;

  for (int i = 1; i < MEASUREMENT_ERROR_COUNT; i++) {
    state.recordError("Testfehler");
    TEST_ASSERT_EQUAL_UINT8(i, state.errorCount);
    TEST_ASSERT_FALSE(state.fatalError);
  }

  state.recordError("Testfehler");
  TEST_ASSERT_EQUAL_UINT8(MEASUREMENT_ERROR_COUNT, state.errorCount);
  TEST_ASSERT_TRUE(state.fatalError);
}

/**
 * Wird der Zähler zurückgesetzt, beginnt die Zählung von vorn.
 *
 * Das ist die Eigenschaft, auf der A2 aufbaut: der Erfolgspfad von
 * handleMeasuring() setzt errorCount und fatalError zurück, damit
 * MEASUREMENT_ERROR_COUNT wieder aufeinanderfolgende Fehler zählt statt über
 * Stunden aufzusummieren.
 */
void test_zuruecksetzen_beginnt_zaehlung_neu() {
  setMillis(1000);
  MeasurementStateInfo state;

  for (int i = 0; i < MEASUREMENT_ERROR_COUNT; i++) {
    state.recordError("Testfehler");
  }
  TEST_ASSERT_TRUE(state.fatalError);

  // Das tut der Erfolgspfad
  state.errorCount = 0;
  state.fatalError = false;

  for (int i = 1; i < MEASUREMENT_ERROR_COUNT; i++) {
    state.recordError("Testfehler");
    TEST_ASSERT_FALSE(state.fatalError);
  }
}

/// reset() räumt Fehler, Aufwärmkennzeichen und Zustand ab.
void test_reset_raeumt_auf() {
  setMillis(1000);
  MeasurementStateInfo state;

  state.recordError("Testfehler");
  state.warmupDoneThisCycle = true;
  state.warmupStartTime = 500;
  state.setState(MeasurementState::MEASURING);

  state.reset();

  TEST_ASSERT_EQUAL_UINT8(0, state.errorCount);
  TEST_ASSERT_FALSE(state.fatalError);
  TEST_ASSERT_FALSE(state.warmupDoneThisCycle);
  TEST_ASSERT_EQUAL_UINT32(0, state.warmupStartTime);
  TEST_ASSERT_TRUE(state.state == MeasurementState::WAITING_FOR_DUE);
}

/**
 * warmupDoneThisCycle muss von warmupStartTime unabhängig sein.
 *
 * Regressionsschutz für die Endlosschleife WARMUP -> WAITING_FOR_DELAY:
 * handleWarmup() setzt warmupStartTime am Ende auf 0, und solange
 * handleWaitingForDelay() diese 0 als "Aufwärmen steht noch aus" las, kam der
 * Sensor nie zum Messen und hielt den Slot bis zur Zwangsfreigabe nach 45 s.
 */
void test_aufwaermkennzeichen_ist_eigenstaendig() {
  setMillis(1000);
  MeasurementStateInfo state;

  state.warmupDoneThisCycle = true;
  state.warmupStartTime = 0; // genau das tut handleWarmup() nach Abschluss

  TEST_ASSERT_TRUE(state.warmupDoneThisCycle);
}

/// setState() merkt sich den Startzeitpunkt - Grundlage der Zeitschranken (A1).
void test_zustandswechsel_merkt_startzeit() {
  setMillis(1000);
  MeasurementStateInfo state;

  advanceMillis(500);
  state.setState(MeasurementState::MEASURING);
  TEST_ASSERT_EQUAL_UINT32(1500, state.stateStartTime);

  advanceMillis(30000);
  // So prüft updateMeasurementCycle() die Zeitschranke
  TEST_ASSERT_TRUE(millis() - state.stateStartTime >= 30000);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_nicht_faellig_vor_dem_zeitpunkt);
  RUN_TEST(test_faellig_ab_dem_zeitpunkt);
  RUN_TEST(test_intervall_null_ist_sofort_faellig);
  RUN_TEST(test_mindestverzoegerung);
  RUN_TEST(test_mindestverzoegerung_null);
  RUN_TEST(test_fehlerzaehler_erreicht_grenze);
  RUN_TEST(test_zuruecksetzen_beginnt_zaehlung_neu);
  RUN_TEST(test_reset_raeumt_auf);
  RUN_TEST(test_aufwaermkennzeichen_ist_eigenstaendig);
  RUN_TEST(test_zustandswechsel_merkt_startzeit);
  return UNITY_END();
}
