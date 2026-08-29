/**
 * @file test_web_auth_decision.cpp
 * @brief Tests für WebAuthDecision::shouldWarnAboutEmergencyPassword()
 *
 * Prüft die Drosselung der Notfallpasswort-Warnung, nicht den
 * Passwortvergleich selbst (der läuft über die ESP8266WebServer-Bibliothek
 * und wird hier bewusst nicht nachgebildet - siehe web_auth_decision.h).
 * Sicherheitsrelevant: zu aggressive Drosselung würde die einzige sichtbare
 * Spur der Notfallpasswort-Nutzung unterdrücken, zu schwache Drosselung
 * würde das Log fluten und die Warnung ihre Signalwirkung verlieren lassen.
 */

#include <unity.h>

#include <Arduino.h>

#include "web/core/web_auth_decision.h"

namespace {
constexpr unsigned long INTERVAL = 60000; // EMERGENCY_WARN_INTERVAL_MS in web_auth.h
}

/// Die allererste Nutzung (lastWarnTime==0) muss immer warnen - sonst bliebe
/// der erste Zugriff über das Notfallpasswort unbemerkt.
void test_erste_nutzung_warnt_immer() {
  unsigned long lastWarn = 0;
  bool result = WebAuthDecision::shouldWarnAboutEmergencyPassword(1000, lastWarn, INTERVAL);

  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_EQUAL_UINT32(1000, lastWarn);
}

/// Unmittelbar danach erneut aufgerufen: keine zweite Warnung innerhalb des
/// Intervalls - das ist der eigentliche Zweck der Drosselung.
void test_wiederholte_nutzung_innerhalb_des_intervalls_warnt_nicht() {
  unsigned long lastWarn = 1000;
  bool result = WebAuthDecision::shouldWarnAboutEmergencyPassword(1001, lastWarn, INTERVAL);

  TEST_ASSERT_FALSE(result);
  // lastWarn darf bei ausbleibender Warnung nicht verändert werden.
  TEST_ASSERT_EQUAL_UINT32(1000, lastWarn);
}

/// Kurz VOR Ablauf des Intervalls: weiterhin keine Warnung.
void test_kurz_vor_intervallende_warnt_nicht() {
  unsigned long lastWarn = 1000;
  bool result =
      WebAuthDecision::shouldWarnAboutEmergencyPassword(1000 + INTERVAL - 1, lastWarn, INTERVAL);

  TEST_ASSERT_FALSE(result);
}

/// Genau am Intervallende: die Bedingung ist ">=", die Grenze selbst zählt
/// schon als "lange genug her".
void test_genau_am_intervallende_warnt() {
  unsigned long lastWarn = 1000;
  bool result =
      WebAuthDecision::shouldWarnAboutEmergencyPassword(1000 + INTERVAL, lastWarn, INTERVAL);

  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_EQUAL_UINT32(1000 + INTERVAL, lastWarn);
}

/// Nach einer Warnung beginnt die Drosselung von vorn: eine dritte Nutzung
/// kurz nach der zweiten Warnung warnt wieder nicht.
void test_drosselung_beginnt_nach_jeder_warnung_neu() {
  unsigned long lastWarn = 0;

  TEST_ASSERT_TRUE(WebAuthDecision::shouldWarnAboutEmergencyPassword(1000, lastWarn, INTERVAL));
  TEST_ASSERT_TRUE(
      WebAuthDecision::shouldWarnAboutEmergencyPassword(1000 + INTERVAL, lastWarn, INTERVAL));

  // Sofort danach: wieder gedrosselt, nicht weil es die erste Warnung ist,
  // sondern weil die zweite gerade erst erfolgt ist.
  bool result =
      WebAuthDecision::shouldWarnAboutEmergencyPassword(1000 + INTERVAL + 1, lastWarn, INTERVAL);
  TEST_ASSERT_FALSE(result);
}

/// Viele schnell aufeinanderfolgende Nutzungen (wie beim Bedienen der
/// Oberfläche, die die Prüfung pro Anfrage mehrfach auslöst) dürfen
/// insgesamt nur eine Handvoll Warnungen erzeugen, nicht eine pro Anfrage.
void test_viele_schnelle_anfragen_erzeugen_nur_eine_warnung() {
  unsigned long lastWarn = 0;
  int warnCount = 0;

  for (int i = 0; i < 50; i++) {
    if (WebAuthDecision::shouldWarnAboutEmergencyPassword(1000 + i, lastWarn, INTERVAL)) {
      warnCount++;
    }
  }

  TEST_ASSERT_EQUAL_INT(1, warnCount);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_erste_nutzung_warnt_immer);
  RUN_TEST(test_wiederholte_nutzung_innerhalb_des_intervalls_warnt_nicht);
  RUN_TEST(test_kurz_vor_intervallende_warnt_nicht);
  RUN_TEST(test_genau_am_intervallende_warnt);
  RUN_TEST(test_drosselung_beginnt_nach_jeder_warnung_neu);
  RUN_TEST(test_viele_schnelle_anfragen_erzeugen_nur_eine_warnung);
  return UNITY_END();
}
