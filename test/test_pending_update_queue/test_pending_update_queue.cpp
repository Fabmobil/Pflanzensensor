/**
 * @file test_pending_update_queue.cpp
 * @brief Tests für PendingUpdateQueue::enqueue() (managers/pending_update_queue.h)
 *
 * Das ist die Write-Behind-Warteschlange, die den Flash-Verschleiß aus P1.4
 * reduziert: Messwerte werden im RAM gesammelt statt bei jeder Änderung
 * einzeln geschrieben. Bisher liefen in jedem anderen Test nur No-op-Stubs
 * dieser Schnittstelle - die eigentliche Warteschlangenlogik (Duplikate
 * zusammenfassen, bei Überlauf den ältesten Eintrag verdrängen) hatte noch
 * keine Testabdeckung.
 */

#include <unity.h>

#include <Arduino.h>

#include "managers/pending_update_queue.h"

namespace {

PendingUpdate makeUpdate(const char* sensorId, size_t measurementIndex, float lastValue) {
  PendingUpdate u{};
  u.type = PendingUpdateType::LAST_VALUE;
  u.sensorId = sensorId;
  u.measurementIndex = measurementIndex;
  u.timestamp = millis();
  u.data.last.lastValue = lastValue;
  u.data.last.lastRawValue = 0;
  return u;
}

} // namespace

/// Ein Eintrag in eine leere Warteschlange wird einfach angehängt, nichts
/// wird verdrängt.
void test_erster_eintrag_wird_angehaengt() {
  std::vector<PendingUpdate> queue;

  auto evicted = PendingUpdateQueue::enqueue(queue, makeUpdate("A", 0, 1.0f), 32);

  TEST_ASSERT_FALSE(evicted.has_value());
  TEST_ASSERT_EQUAL_size_t(1, queue.size());
}

/**
 * Ein zweiter Eintrag mit gleichem (type, sensorId, measurementIndex) ersetzt
 * den vorhandenen, statt einen zweiten Platz zu belegen - sonst würde jede
 * Wertänderung eines Sensors einen eigenen Warteschlangenplatz verbrauchen.
 */
void test_duplikat_ersetzt_vorhandenen_eintrag() {
  std::vector<PendingUpdate> queue;

  PendingUpdateQueue::enqueue(queue, makeUpdate("A", 0, 1.0f), 32);
  auto evicted = PendingUpdateQueue::enqueue(queue, makeUpdate("A", 0, 2.0f), 32);

  TEST_ASSERT_FALSE(evicted.has_value());
  TEST_ASSERT_EQUAL_size_t(1, queue.size());
  TEST_ASSERT_EQUAL_FLOAT(2.0f, queue[0].data.last.lastValue);
}

/// Unterschiedlicher Sensor, unterschiedlicher Messindex und unterschiedlicher
/// Typ zählen alle als eigenständige Einträge, keine Duplikate.
void test_unterschiedliche_schluessel_bleiben_getrennt() {
  std::vector<PendingUpdate> queue;

  PendingUpdateQueue::enqueue(queue, makeUpdate("A", 0, 1.0f), 32);
  PendingUpdateQueue::enqueue(queue, makeUpdate("B", 0, 1.0f), 32); // anderer Sensor
  PendingUpdateQueue::enqueue(queue, makeUpdate("A", 1, 1.0f), 32); // anderer Index

  auto raw = makeUpdate("A", 0, 0.0f);
  raw.type = PendingUpdateType::RAW_MIN_MAX; // anderer Typ, gleicher Sensor+Index
  raw.data.raw.absoluteRawMin = 0;
  raw.data.raw.absoluteRawMax = 1023;
  PendingUpdateQueue::enqueue(queue, raw, 32);

  TEST_ASSERT_EQUAL_size_t(4, queue.size());
}

/// Ist die Warteschlange voll, wird der ÄLTESTE Eintrag verdrängt (FIFO) und
/// zurückgegeben - nicht irgendein beliebiger.
void test_verdraengt_bei_ueberlauf_den_aeltesten() {
  std::vector<PendingUpdate> queue;
  const size_t maxSize = 3;

  PendingUpdateQueue::enqueue(queue, makeUpdate("ERSTER", 0, 1.0f), maxSize);
  PendingUpdateQueue::enqueue(queue, makeUpdate("ZWEITER", 0, 2.0f), maxSize);
  PendingUpdateQueue::enqueue(queue, makeUpdate("DRITTER", 0, 3.0f), maxSize);

  // Warteschlange jetzt voll (3/3). Ein vierter, neuer Eintrag muss Platz machen.
  auto evicted = PendingUpdateQueue::enqueue(queue, makeUpdate("VIERTER", 0, 4.0f), maxSize);

  TEST_ASSERT_TRUE(evicted.has_value());
  TEST_ASSERT_TRUE(evicted->sensorId == "ERSTER");
  TEST_ASSERT_EQUAL_size_t(maxSize, queue.size());

  // Die verbleibenden Einträge sind ZWEITER, DRITTER, VIERTER - in dieser Reihenfolge.
  TEST_ASSERT_TRUE(queue[0].sensorId == "ZWEITER");
  TEST_ASSERT_TRUE(queue[1].sensorId == "DRITTER");
  TEST_ASSERT_TRUE(queue[2].sensorId == "VIERTER");
}

/// Die Warteschlange bleibt dauerhaft auf maxSize gedeckelt, auch über viele
/// Einfügungen hinweg - kein schleichendes Wachstum.
void test_bleibt_dauerhaft_gedeckelt() {
  std::vector<PendingUpdate> queue;
  const size_t maxSize = 5;

  for (int i = 0; i < 100; i++) {
    String id = "S" + String(i);
    PendingUpdateQueue::enqueue(queue, makeUpdate(id.c_str(), 0, static_cast<float>(i)), maxSize);
    TEST_ASSERT_TRUE(queue.size() <= maxSize);
  }

  TEST_ASSERT_EQUAL_size_t(maxSize, queue.size());
  // Nach 100 Einfügungen mit Deckel 5 müssen die letzten 5 (95..99) übrig sein.
  TEST_ASSERT_TRUE(queue[0].sensorId == "S95");
  TEST_ASSERT_TRUE(queue[4].sensorId == "S99");
}

/**
 * Ein Duplikat, das gerade noch VOR dem Überlauf eingereiht wird, darf die
 * Warteschlange nicht wachsen lassen und folglich auch nichts verdrängen -
 * die Dubletten-Prüfung muss der Überlaufprüfung vorausgehen.
 */
void test_duplikat_loest_bei_voller_warteschlange_keine_verdraengung_aus() {
  std::vector<PendingUpdate> queue;
  const size_t maxSize = 2;

  PendingUpdateQueue::enqueue(queue, makeUpdate("A", 0, 1.0f), maxSize);
  PendingUpdateQueue::enqueue(queue, makeUpdate("B", 0, 1.0f), maxSize);
  // Warteschlange jetzt voll (2/2) - aber "A"/0 aktualisieren ist ein Duplikat.
  auto evicted = PendingUpdateQueue::enqueue(queue, makeUpdate("A", 0, 99.0f), maxSize);

  TEST_ASSERT_FALSE(evicted.has_value());
  TEST_ASSERT_EQUAL_size_t(2, queue.size());
  TEST_ASSERT_EQUAL_FLOAT(99.0f, queue[0].data.last.lastValue);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_erster_eintrag_wird_angehaengt);
  RUN_TEST(test_duplikat_ersetzt_vorhandenen_eintrag);
  RUN_TEST(test_unterschiedliche_schluessel_bleiben_getrennt);
  RUN_TEST(test_verdraengt_bei_ueberlauf_den_aeltesten);
  RUN_TEST(test_bleibt_dauerhaft_gedeckelt);
  RUN_TEST(test_duplikat_loest_bei_voller_warteschlange_keine_verdraengung_aus);
  return UNITY_END();
}
