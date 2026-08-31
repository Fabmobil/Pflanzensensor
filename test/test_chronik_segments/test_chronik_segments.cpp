/**
 * @file test_chronik_segments.cpp
 * @brief Tests für Dateinamen und Reihenfolge der Chronik-Segmente
 *
 * Ein hier übersehenes Segment wird nie gelöscht und das Dateisystem läuft
 * langsam voll; ein fälschlich erkanntes /log.txt würde gelöscht. Beides fällt
 * am Gerät erst nach Tagen auf.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/chronik_segments.h"

using namespace ChronikSegments;

void test_name_und_index_gehoeren_zusammen() {
  char name[NAME_BUFFER];
  nameFromIndex(0x2a, name);
  TEST_ASSERT_EQUAL_STRING("/chr_0000002a.dat", name);
  TEST_ASSERT_EQUAL_INT(NAME_LENGTH, strlen(name));
  TEST_ASSERT_EQUAL_INT64(0x2a, indexFromName(name));

  const uint32_t proben[] = {0, 1, 15, 16, 255, 65535, 0x12345678, 0xFFFFFFFF};
  for (size_t i = 0; i < sizeof(proben) / sizeof(proben[0]); i++) {
    nameFromIndex(proben[i], name);
    TEST_ASSERT_EQUAL_INT64(static_cast<int64_t>(proben[i]), indexFromName(name));
  }
}

/// Die Verzeichnisiteration liefert je nach Aufrufweg mit oder ohne
/// führenden Schrägstrich.
void test_name_ohne_schraegstrich_wird_erkannt() {
  TEST_ASSERT_EQUAL_INT64(0x2a, indexFromName("chr_0000002a.dat"));
}

/// Fremde Dateien dürfen nie als Segment durchgehen - sie würden gelöscht.
void test_fremde_dateien_sind_keine_segmente() {
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/log.txt"));
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/log.txt.tmp"));
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/config/settings.json"));
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/chr_xyz.dat"));       // keine Hexziffern
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/chr_0000002a.bak"));  // falsche Endung
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/chr_0000002.dat"));   // zu kurz
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/chr_0000002a.datx")); // zu lang
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/chr_0000002A.dat"));  // Großbuchstaben
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName("/chs_0000002a.dat"));  // anderes Präfix
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName(""));
  TEST_ASSERT_EQUAL_INT64(-1, indexFromName(nullptr));
}

/// Der Verzeichnisdurchlauf liefert die Segmente in beliebiger Reihenfolge.
void test_raender_aus_unsortierter_liste() {
  SegmentRange range;
  TEST_ASSERT_TRUE(range.empty());
  TEST_ASSERT_EQUAL_UINT32(0, range.nextIndex());

  const uint32_t gefunden[] = {7, 3, 9, 4, 8};
  for (size_t i = 0; i < sizeof(gefunden) / sizeof(gefunden[0]); i++) {
    range.add(gefunden[i]);
  }

  TEST_ASSERT_FALSE(range.empty());
  TEST_ASSERT_EQUAL_UINT16(5, range.count());
  TEST_ASSERT_EQUAL_UINT32(3, range.oldest());
  TEST_ASSERT_EQUAL_UINT32(9, range.newest());
  TEST_ASSERT_EQUAL_UINT32(10, range.nextIndex());
}

/// Nach Löschungen bleiben Lücken - das darf die Ränder nicht verwirren.
void test_luecken_stoeren_die_raender_nicht() {
  SegmentRange range;
  range.add(100);
  range.add(103); // 101 und 102 wurden gelöscht
  range.add(104);
  TEST_ASSERT_EQUAL_UINT32(100, range.oldest());
  TEST_ASSERT_EQUAL_UINT32(104, range.newest());
  TEST_ASSERT_EQUAL_UINT16(3, range.count());

  range.reset();
  TEST_ASSERT_TRUE(range.empty());
  TEST_ASSERT_EQUAL_UINT32(0, range.nextIndex());
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_name_und_index_gehoeren_zusammen);
  RUN_TEST(test_name_ohne_schraegstrich_wird_erkannt);
  RUN_TEST(test_fremde_dateien_sind_keine_segmente);
  RUN_TEST(test_raender_aus_unsortierter_liste);
  RUN_TEST(test_luecken_stoeren_die_raender_nicht);
  return UNITY_END();
}
