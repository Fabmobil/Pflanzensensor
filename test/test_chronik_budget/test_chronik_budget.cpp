/**
 * @file test_chronik_budget.cpp
 * @brief Tests für die Platzrechnung der Chronik (utils/chronik_budget.h)
 *
 * Diese Rechnung entscheidet, wieviele Segmente gelöscht werden. Ist sie zu
 * großzügig, läuft die Rotation des Datei-Logs in ein volles Dateisystem und
 * reißt die Konfigurationsschreibvorgänge mit; ist sie zu knapp, verschenkt
 * das Gerät Historie. Die Zahlen stammen vom laufenden Gerät (172.17.1.44:
 * 928 KB gesamt, 688 KB belegt).
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/chronik_budget.h"

using namespace ChronikBudget;

namespace {
/// Freier Platz auf dem Gerät, nachdem die neuen Web-Dateien im Abbild liegen.
constexpr uint32_t FREI_AUF_GERAET = 221184; // 216 KB
} // namespace

/// Der Normalfall: Datei-Logging aus, Chronik noch leer.
void test_geraetezahlen_ergeben_zwanzig_segmente() {
  Input in;
  in.freeBytes = FREI_AUF_GERAET;
  in.ownBytes = 0;
  in.fileLogEnabled = false;
  TEST_ASSERT_EQUAL_UINT8(20, targetSegments(in));
}

/// Datei-Logging braucht die Rotationsspitze - die Chronik muss zurückweichen.
void test_datei_log_verkleinert_das_fenster() {
  Input in;
  in.freeBytes = FREI_AUF_GERAET;
  in.ownBytes = 0;
  in.fileLogEnabled = true;
  TEST_ASSERT_EQUAL_UINT8(12, targetSegments(in));
}

/// Der eigene Verbrauch zählt zum verfügbaren Platz: sonst schrumpfte das
/// Fenster bei jeder Prüfung weiter, weil es sich selbst wegrechnet.
void test_eigener_verbrauch_zaehlt_mit() {
  Input voll;
  voll.freeBytes = 63488; // 8 Segmente frei
  voll.ownBytes = 20 * SEGMENT_SIZE;
  voll.fileLogEnabled = false;

  Input leer;
  leer.freeBytes = 63488 + 20 * SEGMENT_SIZE;
  leer.ownBytes = 0;
  leer.fileLogEnabled = false;

  TEST_ASSERT_EQUAL_UINT8(targetSegments(leer), targetSegments(voll));
}

/// Der klassische Unterlauf: weniger frei als die Reserve. Vorzeichenlos
/// gerechnet käme hier eine Milliardenzahl heraus und die Chronik würde das
/// Dateisystem auffressen.
void test_zu_wenig_platz_ergibt_null_segmente() {
  Input in;
  in.freeBytes = 1000;
  in.ownBytes = 0;
  in.fileLogEnabled = false;
  TEST_ASSERT_EQUAL_UINT8(0, targetSegments(in));

  in.freeBytes = reserveBytes(false); // exakt die Reserve
  TEST_ASSERT_EQUAL_UINT8(0, targetSegments(in));

  in.freeBytes = reserveBytes(false) + SEGMENT_SIZE;
  TEST_ASSERT_EQUAL_UINT8(1, targetSegments(in));
}

/// Auch auf einem Gerät mit riesigem Dateisystem bleibt es beim Deckel.
void test_obergrenze_greift() {
  Input in;
  in.freeBytes = 8UL * 1024 * 1024;
  in.ownBytes = 0;
  in.fileLogEnabled = false;
  TEST_ASSERT_EQUAL_UINT8(MAX_SEGMENTS, targetSegments(in));
}

/// Der Fall, der beim Einschalten des Datei-Logs eintritt.
void test_ueberzaehlige_segmente() {
  TEST_ASSERT_EQUAL_UINT8(8, excessSegments(20, 12));
  TEST_ASSERT_EQUAL_UINT8(0, excessSegments(12, 12));
  TEST_ASSERT_EQUAL_UINT8(0, excessSegments(5, 12)); // kein Unterlauf
  TEST_ASSERT_EQUAL_UINT8(20, excessSegments(20, 0));
}

/// Die Reserve muss beim Einschalten des Datei-Logs um dessen Spitzenbedarf
/// wachsen, sonst wäre die Fallunterscheidung wirkungslos.
void test_reserve_beruecksichtigt_das_datei_log() {
  TEST_ASSERT_EQUAL_UINT32(RESERVE_FILE_LOG_OFF + RESERVE_CONFIG + RESERVE_COMPACT,
                           reserveBytes(false));
  TEST_ASSERT_EQUAL_UINT32(RESERVE_FILE_LOG_ON + RESERVE_CONFIG + RESERVE_COMPACT,
                           reserveBytes(true));
  TEST_ASSERT_GREATER_THAN_UINT32(reserveBytes(false), reserveBytes(true));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_geraetezahlen_ergeben_zwanzig_segmente);
  RUN_TEST(test_datei_log_verkleinert_das_fenster);
  RUN_TEST(test_eigener_verbrauch_zaehlt_mit);
  RUN_TEST(test_zu_wenig_platz_ergibt_null_segmente);
  RUN_TEST(test_obergrenze_greift);
  RUN_TEST(test_ueberzaehlige_segmente);
  RUN_TEST(test_reserve_beruecksichtigt_das_datei_log);
  return UNITY_END();
}
