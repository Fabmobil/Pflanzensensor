/**
 * @file test_chronik_format.cpp
 * @brief Tests für das Rahmenformat der Chronik (utils/chronik_format.h)
 *
 * Getestet wird die echte Implementierung. Die Referenzwerte stammen aus
 * Pythons struct-Modul ('e' = IEEE-754 half, '<H'/'<I' little-endian) und
 * einer eigens ausgerechneten CRC-8 - also unabhängig von diesem Code.
 *
 * Der Formatvergleich in test_formatarretierung ist der wichtigste Test der
 * Datei: dieselbe Bytefolge steht im JavaScript-Test (test/js/chronik.test.mjs).
 * Ändert eine Seite das Format, fällt es hier und dort auf, statt erst am
 * Gerät als unlesbare Chronik.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/chronik_format.h"

using namespace ChronikFormat;

namespace {

/// Rahmen, dessen Bytefolge in Python unabhängig nachgerechnet wurde.
SampleFrame referenzRahmen() {
  SampleFrame frame;
  frame.epoch = 1756612345UL;
  frame.count = 2;
  frame.values[0] = {0, STATUS_GREEN, 33.2f, true, 658};
  frame.values[1] = {3, STATUS_YELLOW, -12.5f, false, -1};
  return frame;
}

/// Sammelt die Einträge aus readTable(), inklusive Kopie der nicht
/// nullterminierten Texte.
struct Gelesen {
  TableEntry eintraege[MAX_CHANNELS];
  char texte[MAX_CHANNELS][3][MAX_TEXT + 1];
  uint8_t anzahl{0};
};

void sammle(void* context, const TableEntry& entry) {
  Gelesen* g = static_cast<Gelesen*>(context);
  if (g->anzahl >= MAX_CHANNELS)
    return;
  const uint8_t i = g->anzahl;
  g->eintraege[i] = entry;
  const char* quellen[3] = {entry.key, entry.name, entry.unit};
  const uint8_t laengen[3] = {entry.keyLength, entry.nameLength, entry.unitLength};
  for (uint8_t t = 0; t < 3; t++) {
    memcpy(g->texte[i][t], quellen[t], laengen[t]);
    g->texte[i][t][laengen[t]] = '\0';
  }
  g->anzahl++;
}

const uint8_t REFERENZ_BYTES[] = {0xA5, 0xC5, 0xF9, 0xC6, 0xB3, 0x68, 0x02, 0x80,
                                  0x26, 0x50, 0x92, 0x02, 0x13, 0x40, 0xCA, 0xE0};

} // namespace

// === Halbe Fließkommazahlen ===

/// Werte, die sich exakt als half darstellen lassen, müssen bitgenau
/// zurückkommen - sonst driftete jede Kurve systematisch.
void test_half_exakte_werte_ueberleben_den_roundtrip() {
  const float werte[] = {0.0f, 1.0f, -1.0f, 0.5f, 100.0f, 5000.0f, 65504.0f, -12.5f};
  for (size_t i = 0; i < sizeof(werte) / sizeof(werte[0]); i++) {
    TEST_ASSERT_EQUAL_FLOAT(werte[i], floatFromHalf(halfFromFloat(werte[i])));
  }
}

/// Bitmuster gegen Pythons struct.pack('<e', ...) - das ist die eigentliche
/// Zusicherung, dass hier IEEE-754 half herauskommt und nicht etwas Ähnliches.
void test_half_bitmuster_entspricht_ieee754() {
  TEST_ASSERT_EQUAL_HEX16(0x0000, halfFromFloat(0.0f));
  TEST_ASSERT_EQUAL_HEX16(0x8000, halfFromFloat(-0.0f));
  TEST_ASSERT_EQUAL_HEX16(0x3C00, halfFromFloat(1.0f));
  TEST_ASSERT_EQUAL_HEX16(0xBC00, halfFromFloat(-1.0f));
  TEST_ASSERT_EQUAL_HEX16(0x3800, halfFromFloat(0.5f));
  TEST_ASSERT_EQUAL_HEX16(0x5640, halfFromFloat(100.0f));
  TEST_ASSERT_EQUAL_HEX16(0x6CE2, halfFromFloat(5000.0f));
  TEST_ASSERT_EQUAL_HEX16(0x7BFF, halfFromFloat(65504.0f));
  TEST_ASSERT_EQUAL_HEX16(0x4E58, halfFromFloat(25.37f));
  TEST_ASSERT_EQUAL_HEX16(0x63EA, halfFromFloat(1013.25f));
  TEST_ASSERT_EQUAL_HEX16(0x5026, halfFromFloat(33.2f));
  TEST_ASSERT_EQUAL_HEX16(0xCA40, halfFromFloat(-12.5f));
}

/// Die Quantisierung ist der Preis für die halbe Dateigröße. Sie muss klein
/// genug bleiben, dass sie in einem Diagramm nicht sichtbar wird.
void test_half_quantisierung_bleibt_im_rahmen() {
  TEST_ASSERT_FLOAT_WITHIN(0.016f, 25.37f, floatFromHalf(halfFromFloat(25.37f)));
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 33.2f, floatFromHalf(halfFromFloat(33.2f)));
  // Luftdruck ist der ungünstigste realistische Fall - dokumentierte Grenze.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 1013.25f, floatFromHalf(halfFromFloat(1013.25f)));
}

/// Randfälle dürfen keine stillen Zahlensalate ergeben.
void test_half_randfaelle() {
  TEST_ASSERT_EQUAL_HEX16(0x0000, halfFromFloat(1e-8f));      // zu klein -> null
  TEST_ASSERT_EQUAL_HEX16(0x7C00, halfFromFloat(70000.0f));   // zu groß -> Inf
  TEST_ASSERT_EQUAL_HEX16(0xFC00, halfFromFloat(-70000.0f));  // zu klein -> -Inf
  TEST_ASSERT_TRUE(isnan(floatFromHalf(halfFromFloat(NAN)))); // NaN bleibt NaN
  // Subnormaler Bereich: darf nicht auf null zusammenfallen
  TEST_ASSERT_FLOAT_WITHIN(1e-7f, 1e-5f, floatFromHalf(halfFromFloat(1e-5f)));
}

// === Prüfsumme ===

/// Der klassische Prüfwert für CRC-8/ATM, unabhängig von diesem Code.
void test_crc8_standardpruefwert() {
  const char* text = "123456789";
  TEST_ASSERT_EQUAL_HEX8(0xF4, crc8(reinterpret_cast<const uint8_t*>(text), 9));
}

void test_crc8_leere_eingabe_ist_null() { TEST_ASSERT_EQUAL_HEX8(0x00, crc8(nullptr, 0)); }

// === Rahmen ===

/// Bytegenauer Vergleich gegen die in Python nachgerechnete Folge. Diese
/// Bytefolge steht identisch im JavaScript-Test.
void test_formatarretierung() {
  uint8_t puffer[64] = {0};
  const size_t n = writeSample(referenzRahmen(), puffer, sizeof(puffer));
  TEST_ASSERT_EQUAL_UINT32(sizeof(REFERENZ_BYTES), n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(REFERENZ_BYTES, puffer, sizeof(REFERENZ_BYTES));
}

void test_messrahmen_roundtrip_mit_allen_statuswerten() {
  for (uint8_t status = 0; status <= STATUS_WARMUP; status++) {
    for (uint8_t anzahl = 1; anzahl <= 8; anzahl++) {
      SampleFrame frame;
      frame.epoch = 1700000000UL + status;
      frame.count = anzahl;
      for (uint8_t i = 0; i < anzahl; i++) {
        frame.values[i] = {i, status, 10.0f + i, (i % 2) == 0, static_cast<int16_t>(100 * i)};
      }

      uint8_t puffer[256] = {0};
      const size_t n = writeSample(frame, puffer, sizeof(puffer));
      TEST_ASSERT_GREATER_THAN_UINT32(0, n);
      TEST_ASSERT_EQUAL_UINT32(n, sampleFrameSize(frame));

      SampleFrame zurueck;
      TEST_ASSERT_EQUAL_UINT32(n, readSample(puffer, n, zurueck));
      TEST_ASSERT_EQUAL_UINT32(frame.epoch, zurueck.epoch);
      TEST_ASSERT_EQUAL_UINT8(anzahl, zurueck.count);
      for (uint8_t i = 0; i < anzahl; i++) {
        TEST_ASSERT_EQUAL_UINT8(frame.values[i].channel, zurueck.values[i].channel);
        TEST_ASSERT_EQUAL_UINT8(status, zurueck.values[i].status);
        TEST_ASSERT_EQUAL_FLOAT(frame.values[i].value, zurueck.values[i].value);
        TEST_ASSERT_EQUAL(frame.values[i].hasRaw, zurueck.values[i].hasRaw);
        if (frame.values[i].hasRaw) {
          TEST_ASSERT_EQUAL_INT16(frame.values[i].raw, zurueck.values[i].raw);
        } else {
          TEST_ASSERT_EQUAL_INT16(-1, zurueck.values[i].raw);
        }
      }
    }
  }
}

/// Die Kanaltabelle steht am Anfang jedes Segments und macht es für sich
/// lesbar - Texte und Schwellwerte müssen unverändert zurückkommen.
void test_kanaltabelle_roundtrip() {
  uint8_t puffer[256] = {0};
  TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
  builder.addChannel(0, true, "ANALOG_0", "Lichtstaerke", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  builder.addChannel(2, false, "DHT_0", "Lufttemperatur",
                     "\xC2\xB0"
                     "C",
                     10.0f, 15.0f, 25.0f, 30.0f);
  const size_t n = builder.finish();
  TEST_ASSERT_GREATER_THAN_UINT32(0, n);
  TEST_ASSERT_EQUAL_UINT8(2, builder.count());

  Gelesen gelesen;
  TEST_ASSERT_EQUAL_UINT32(n, readTable(puffer, n, sammle, &gelesen));
  TEST_ASSERT_EQUAL_UINT8(2, gelesen.anzahl);

  TEST_ASSERT_EQUAL_UINT8(0, gelesen.eintraege[0].channel);
  TEST_ASSERT_TRUE(gelesen.eintraege[0].analog);
  TEST_ASSERT_EQUAL_STRING("ANALOG_0", gelesen.texte[0][0]);
  TEST_ASSERT_EQUAL_STRING("Lichtstaerke", gelesen.texte[0][1]);
  TEST_ASSERT_EQUAL_STRING("%", gelesen.texte[0][2]);
  TEST_ASSERT_EQUAL_FLOAT(80.0f, gelesen.eintraege[0].greenHigh);

  TEST_ASSERT_EQUAL_UINT8(2, gelesen.eintraege[1].channel);
  TEST_ASSERT_FALSE(gelesen.eintraege[1].analog);
  TEST_ASSERT_EQUAL_STRING("DHT_0", gelesen.texte[1][0]);
  TEST_ASSERT_EQUAL_STRING("\xC2\xB0"
                           "C",
                           gelesen.texte[1][2]);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, gelesen.eintraege[1].yellowHigh);

  // frameLength kennt beide Rahmentypen, ohne sie zu entpacken
  TEST_ASSERT_EQUAL_UINT32(n, frameLength(puffer, n));
}

/// Ein beschädigter Rahmen darf keine halben Einträge melden - der Aufrufer
/// bekäme sonst Daten aus einem Rahmen, der sich hinterher als kaputt erweist.
void test_kaputte_kanaltabelle_meldet_nichts() {
  uint8_t puffer[256] = {0};
  TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
  builder.addChannel(0, true, "ANALOG_0", "Lichtstaerke", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  builder.addChannel(1, true, "ANALOG_1", "Bodenfeuchte", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  const size_t n = builder.finish();

  puffer[20] ^= 0x04; // ein Bit im zweiten Eintrag kippen

  Gelesen gelesen;
  TEST_ASSERT_EQUAL_UINT32(0, readTable(puffer, n, sammle, &gelesen));
  TEST_ASSERT_EQUAL_UINT8(0, gelesen.anzahl);
}

/// Reicht der Platz nicht, darf finish() keinen Rahmen ausgeben.
void test_kanaltabelle_ohne_platz() {
  uint8_t puffer[24] = {0};
  TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
  builder.addChannel(0, true, "ANALOG_0", "Lichtstaerke", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  TEST_ASSERT_EQUAL_UINT32(0, builder.finish());

  // Ohne einen einzigen Kanal ebenfalls nicht
  uint8_t gross[128] = {0};
  TableBuilder leer(gross, sizeof(gross), 1756612345UL);
  TEST_ASSERT_EQUAL_UINT32(0, leer.finish());
}

/// Passt der Rahmen nicht mehr in den Puffer, darf nichts geschrieben werden -
/// sonst stünde ein Fragment in der Datei, das kein Leser einordnen kann.
void test_zu_kleiner_puffer_schreibt_nichts() {
  uint8_t puffer[32];
  memset(puffer, 0xAA, sizeof(puffer));
  const SampleFrame frame = referenzRahmen();
  TEST_ASSERT_EQUAL_UINT32(0, writeSample(frame, puffer, sampleFrameSize(frame) - 1));
  for (size_t i = 0; i < sizeof(puffer); i++) {
    TEST_ASSERT_EQUAL_HEX8(0xAA, puffer[i]); // kein einziges Byte angefasst
  }
}

/// Ein abgeschnittener Rahmen (Stromausfall, abgebrochener Datenstrom) muss an
/// JEDER Schnittstelle erkannt werden, und readFrame darf nie über die
/// angegebene Länge hinauslesen.
void test_jedes_praefix_wird_abgelehnt() {
  uint8_t puffer[64];
  memset(puffer, 0x5A, sizeof(puffer)); // Wächterbytes hinter dem Rahmen
  const size_t n = writeSample(referenzRahmen(), puffer, sizeof(puffer));

  SampleFrame zurueck;
  for (size_t len = 0; len < n; len++) {
    TEST_ASSERT_EQUAL_UINT32(0, readSample(puffer, len, zurueck));
  }
  TEST_ASSERT_EQUAL_UINT32(n, readSample(puffer, n, zurueck));
}

/// Ein gekipptes Bit darf keinen scheinbar gültigen Rahmen ergeben.
void test_bitfehler_werden_erkannt() {
  uint8_t original[64] = {0};
  const size_t n = writeSample(referenzRahmen(), original, sizeof(original));

  size_t durchgelassen = 0;
  for (size_t byte = 0; byte < n; byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      uint8_t kopie[64];
      memcpy(kopie, original, sizeof(kopie));
      kopie[byte] ^= static_cast<uint8_t>(1 << bit);

      SampleFrame zurueck;
      if (readSample(kopie, n, zurueck) != 0) {
        durchgelassen++;
      }
    }
  }
  // CRC-8 lässt rechnerisch etwa 1/256 der Störungen durch; bei 128 geprüften
  // Kippungen darf höchstens eine übrig bleiben.
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(1, durchgelassen);
}

void test_falsches_magic_wird_abgelehnt() {
  uint8_t puffer[64] = {0};
  const size_t n = writeSample(referenzRahmen(), puffer, sizeof(puffer));
  puffer[0] = 0x00;
  SampleFrame zurueck;
  TEST_ASSERT_EQUAL_UINT32(0, readSample(puffer, n, zurueck));
  TEST_ASSERT_EQUAL_UINT32(0, frameLength(puffer, n));
}

/// Nach einer Störstelle muss der Leser wieder aufsetzen können, sonst kostet
/// ein einzelnes kaputtes Byte den Rest des Segments.
void test_findet_magic_nach_muell() {
  uint8_t puffer[128];
  memset(puffer, 0x37, sizeof(puffer));
  const size_t muell = 37;
  const size_t n = writeSample(referenzRahmen(), puffer + muell, sizeof(puffer) - muell);
  TEST_ASSERT_GREATER_THAN_UINT32(0, n);

  TEST_ASSERT_EQUAL_UINT32(muell, findMagic(puffer, muell + n, 0));

  // Ein einzelnes Byte am Pufferende ist kein Magic
  uint8_t knapp[3] = {0x00, 0xA5, 0xC5};
  TEST_ASSERT_EQUAL_UINT32(1, findMagic(knapp, sizeof(knapp), 0));
  uint8_t abgeschnitten[2] = {0x00, 0xA5};
  TEST_ASSERT_EQUAL_UINT32(sizeof(abgeschnitten),
                           findMagic(abgeschnitten, sizeof(abgeschnitten), 0));
}

/// Der Startfall: drei vollständige Rahmen, dann ein angeschnittener. Genau
/// bis zum Ende des dritten wird die Datei gekürzt.
void test_gueltige_laenge_endet_vor_dem_bruch() {
  uint8_t puffer[256] = {0};
  size_t at = 0;
  size_t nachDrei = 0;
  for (uint8_t i = 0; i < 3; i++) {
    SampleFrame frame = referenzRahmen();
    frame.epoch += i;
    at += writeSample(frame, puffer + at, sizeof(puffer) - at);
    nachDrei = at;
  }
  const size_t vierter = writeSample(referenzRahmen(), puffer + at, sizeof(puffer) - at);
  TEST_ASSERT_GREATER_THAN_UINT32(1, vierter);

  // Datei endet mitten im vierten Rahmen
  TEST_ASSERT_EQUAL_UINT32(nachDrei, validPrefixLength(puffer, at + vierter - 2));
  // Unversehrt: alle vier zählen
  TEST_ASSERT_EQUAL_UINT32(at + vierter, validPrefixLength(puffer, at + vierter));
  TEST_ASSERT_EQUAL_UINT32(0, validPrefixLength(puffer, 0));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_half_exakte_werte_ueberleben_den_roundtrip);
  RUN_TEST(test_half_bitmuster_entspricht_ieee754);
  RUN_TEST(test_half_quantisierung_bleibt_im_rahmen);
  RUN_TEST(test_half_randfaelle);
  RUN_TEST(test_crc8_standardpruefwert);
  RUN_TEST(test_crc8_leere_eingabe_ist_null);
  RUN_TEST(test_formatarretierung);
  RUN_TEST(test_messrahmen_roundtrip_mit_allen_statuswerten);
  RUN_TEST(test_kanaltabelle_roundtrip);
  RUN_TEST(test_kaputte_kanaltabelle_meldet_nichts);
  RUN_TEST(test_kanaltabelle_ohne_platz);
  RUN_TEST(test_zu_kleiner_puffer_schreibt_nichts);
  RUN_TEST(test_jedes_praefix_wird_abgelehnt);
  RUN_TEST(test_bitfehler_werden_erkannt);
  RUN_TEST(test_falsches_magic_wird_abgelehnt);
  RUN_TEST(test_findet_magic_nach_muell);
  RUN_TEST(test_gueltige_laenge_endet_vor_dem_bruch);
  return UNITY_END();
}
