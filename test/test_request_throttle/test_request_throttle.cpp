/**
 * @file test_request_throttle.cpp
 * @brief Tests für RequestThrottle (web/core/request_throttle.h)
 *
 * Getestet wird die echte, unveränderte Implementierung. Die Drossel schützt
 * den offenen Endpunkt POST /measure, den jeder im WLAN ohne Anmeldung
 * aufrufen kann - ihre Grenzfälle (erste Anfrage nach dem Start, Überlauf von
 * millis()) sind am Gerät praktisch nicht zu provozieren, hier schon.
 */

#include <unity.h>

#include <Arduino.h>

#include "web/core/request_throttle.h"

namespace {
constexpr uint32_t MIN_INTERVAL = 2000;
} // namespace

/// Direkt nach dem Einschalten steht millis() bei 0 - genau dem Startwert von
/// m_last. Ohne das m_hasFired-Kennzeichen gälte das als "gerade eben
/// durchgelassen" und der erste Klick auf der Startseite ginge ins Leere.
void test_erste_anfrage_nach_dem_start_geht_durch() {
  RequestThrottle throttle(MIN_INTERVAL);
  TEST_ASSERT_TRUE(throttle.tryAcquire(0));
}

/// Der eigentliche Zweck: zwei Anfragen dicht hintereinander, die zweite fällt raus.
void test_zweite_anfrage_innerhalb_der_sperre_wird_abgewiesen() {
  RequestThrottle throttle(MIN_INTERVAL);
  TEST_ASSERT_TRUE(throttle.tryAcquire(10000));
  TEST_ASSERT_FALSE(throttle.tryAcquire(10500));
  TEST_ASSERT_FALSE(throttle.tryAcquire(11999));
}

/// Nach Ablauf des Mindestabstands ist wieder frei.
void test_nach_ablauf_wieder_frei() {
  RequestThrottle throttle(MIN_INTERVAL);
  TEST_ASSERT_TRUE(throttle.tryAcquire(10000));
  TEST_ASSERT_TRUE(throttle.tryAcquire(12000));
}

/// Abgewiesene Anfragen setzen den Zeitstempel nicht neu. Sonst könnte wer im
/// Sekundentakt anklopft die Sperre endlos verlängern und käme nie durch.
void test_abgewiesene_anfrage_verlaengert_die_sperre_nicht() {
  RequestThrottle throttle(MIN_INTERVAL);
  TEST_ASSERT_TRUE(throttle.tryAcquire(10000));
  TEST_ASSERT_FALSE(throttle.tryAcquire(11000));
  TEST_ASSERT_FALSE(throttle.tryAcquire(11500));
  TEST_ASSERT_TRUE(throttle.tryAcquire(12000)); // 2 s nach der ersten, nicht nach der letzten
}

/// remaining() liefert die Restzeit, die der Server als retryAfterMs
/// mitschickt - die Oberfläche wartet danach genau so lange.
void test_restzeit_wird_korrekt_berechnet() {
  RequestThrottle throttle(MIN_INTERVAL);
  TEST_ASSERT_EQUAL_UINT32(0, throttle.remaining(10000)); // vor der ersten Anfrage frei
  throttle.tryAcquire(10000);
  TEST_ASSERT_EQUAL_UINT32(2000, throttle.remaining(10000));
  TEST_ASSERT_EQUAL_UINT32(500, throttle.remaining(11500));
  TEST_ASSERT_EQUAL_UINT32(0, throttle.remaining(12000));
  TEST_ASSERT_EQUAL_UINT32(0, throttle.remaining(99000));
}

/// millis() läuft nach 49,7 Tagen über. Die Differenzrechnung in unsigned long
/// liefert über den Wrap hinweg weiterhin die tatsächlich vergangene Zeit -
/// ein Gerät, das lange läuft, sperrt danach also weder dauerhaft noch gar nicht.
void test_ueberlauf_von_millis() {
  RequestThrottle throttle(MIN_INTERVAL);
  const uint32_t kurzVorUeberlauf = 0xFFFFFF00UL; // 256 ms vor dem Wrap
  TEST_ASSERT_TRUE(throttle.tryAcquire(kurzVorUeberlauf));

  TEST_ASSERT_FALSE(throttle.tryAcquire(500)); // 756 ms später (nach dem Wrap): noch gesperrt
  TEST_ASSERT_EQUAL_UINT32(1244, throttle.remaining(500));
  TEST_ASSERT_TRUE(throttle.tryAcquire(1744)); // exakt 2000 ms nach der ersten Anfrage
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_erste_anfrage_nach_dem_start_geht_durch);
  RUN_TEST(test_zweite_anfrage_innerhalb_der_sperre_wird_abgewiesen);
  RUN_TEST(test_nach_ablauf_wieder_frei);
  RUN_TEST(test_abgewiesene_anfrage_verlaengert_die_sperre_nicht);
  RUN_TEST(test_restzeit_wird_korrekt_berechnet);
  RUN_TEST(test_ueberlauf_von_millis);
  return UNITY_END();
}
