/**
 * @file test_limiter.cpp
 * @brief Tests für SensorManagerLimiter (Vergabe des Messslots)
 *
 * Hier saß der Fehler, der eine manuell ausgelöste Messung 42 Sekunden lang
 * hängen ließ: acquireSlot() lieferte false, wenn der anfragende Sensor den
 * Slot bereits selbst hielt. Gefunden wurde er durch Messen am Gerät - ein
 * Test hätte ihn in Millisekunden gezeigt.
 */

#include <unity.h>

#include <Arduino.h>

#include "sensors/sensor_manager_limiter.h"

namespace {

/// Der Limiter ist ein Singleton und behält seinen Zustand zwischen Tests.
void releaseAnySlot() {
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();
  const String holder = limiter.getCurrentSensor();
  if (!holder.isEmpty()) {
    limiter.releaseSlot(holder);
  }
}

void setUpClean(unsigned long now = 1000) {
  setMillis(now);
  releaseAnySlot();
}

} // namespace

/// Ein freier Slot wird vergeben.
void test_freier_slot_wird_vergeben() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  TEST_ASSERT_TRUE(limiter.acquireSlot("ANALOG"));
  TEST_ASSERT_TRUE(limiter.hasSlot("ANALOG"));
}

/// Ein zweiter Sensor bekommt den belegten Slot nicht.
void test_belegter_slot_blockiert_andere() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  TEST_ASSERT_TRUE(limiter.acquireSlot("ANALOG"));
  TEST_ASSERT_FALSE(limiter.acquireSlot("DHT"));
  TEST_ASSERT_TRUE(limiter.hasSlot("ANALOG"));
}

/**
 * Der Halter darf seinen eigenen Slot erneut anfordern.
 *
 * Das ist der Regressionstest zum 42-Sekunden-Fehler: vorher lieferte
 * acquireSlot() hier false, der Sensor sperrte sich selbst aus und kam erst
 * nach der Zwangsfreigabe (45 s) wieder zum Zug.
 */
void test_halter_darf_eigenen_slot_erneut_anfordern() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  TEST_ASSERT_TRUE(limiter.acquireSlot("ANALOG"));
  TEST_ASSERT_TRUE(limiter.acquireSlot("ANALOG"));
  TEST_ASSERT_TRUE(limiter.hasSlot("ANALOG"));
}

/// Freigeben macht den Slot für andere verfügbar.
void test_freigabe_gibt_slot_frei() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  limiter.acquireSlot("ANALOG");
  limiter.releaseSlot("ANALOG");

  TEST_ASSERT_FALSE(limiter.hasSlot("ANALOG"));
  TEST_ASSERT_TRUE(limiter.acquireSlot("DHT"));
}

/// Ein Fremder kann den Slot nicht freigeben.
void test_fremde_freigabe_wirkt_nicht() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  limiter.acquireSlot("ANALOG");
  limiter.releaseSlot("DHT"); // nicht der Halter

  TEST_ASSERT_TRUE(limiter.hasSlot("ANALOG"));
}

/// Nach Ablauf von SLOT_TIMEOUT_MS wird der Slot zwangsweise freigegeben.
void test_zwangsfreigabe_nach_zeitueberschreitung() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  limiter.acquireSlot("ANALOG");
  TEST_ASSERT_FALSE(limiter.acquireSlot("DHT"));

  advanceMillis(SensorManagerLimiter::SLOT_TIMEOUT_MS);

  TEST_ASSERT_TRUE(limiter.acquireSlot("DHT"));
  TEST_ASSERT_TRUE(limiter.hasSlot("DHT"));
}

/// Kurz vor Ablauf bleibt der Slot beim Halter.
void test_kein_vorzeitiger_entzug() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  limiter.acquireSlot("ANALOG");
  advanceMillis(SensorManagerLimiter::SLOT_TIMEOUT_MS - 1);

  TEST_ASSERT_FALSE(limiter.acquireSlot("DHT"));
  TEST_ASSERT_TRUE(limiter.hasSlot("ANALOG"));
}

/// forceTakeSlot() entzieht den Slot - Grundlage der manuell ausgelösten Messung.
void test_erzwungene_uebernahme() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  limiter.acquireSlot("ANALOG");
  limiter.forceTakeSlot("DHT");

  TEST_ASSERT_TRUE(limiter.hasSlot("DHT"));
  TEST_ASSERT_FALSE(limiter.hasSlot("ANALOG"));
}

/// Nach erzwungener Übernahme läuft die Haltezeit neu.
void test_uebernahme_setzt_haltezeit_zurueck() {
  setUpClean();
  SensorManagerLimiter& limiter = SensorManagerLimiter::getInstance();

  limiter.acquireSlot("ANALOG");
  advanceMillis(SensorManagerLimiter::SLOT_TIMEOUT_MS - 100);
  limiter.forceTakeSlot("DHT");

  // Wäre die Haltezeit nicht zurückgesetzt, entzöge die Zeitüberschreitung
  // dem neuen Halter den Slot schon nach 100 ms wieder.
  advanceMillis(200);
  TEST_ASSERT_FALSE(limiter.acquireSlot("ANALOG"));
  TEST_ASSERT_TRUE(limiter.hasSlot("DHT"));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_freier_slot_wird_vergeben);
  RUN_TEST(test_belegter_slot_blockiert_andere);
  RUN_TEST(test_halter_darf_eigenen_slot_erneut_anfordern);
  RUN_TEST(test_freigabe_gibt_slot_frei);
  RUN_TEST(test_fremde_freigabe_wirkt_nicht);
  RUN_TEST(test_zwangsfreigabe_nach_zeitueberschreitung);
  RUN_TEST(test_kein_vorzeitiger_entzug);
  RUN_TEST(test_erzwungene_uebernahme);
  RUN_TEST(test_uebernahme_setzt_haltezeit_zurueck);
  return UNITY_END();
}
