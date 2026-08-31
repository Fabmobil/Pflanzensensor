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
  builder.addChannel(0, true, false, "ANALOG_0", "Lichtstaerke", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  builder.addChannel(2, false, false, "DHT_0", "Lufttemperatur",
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
  builder.addChannel(0, true, false, "ANALOG_0", "Lichtstaerke", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  builder.addChannel(1, true, false, "ANALOG_1", "Bodenfeuchte", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  const size_t n = builder.finish();

  puffer[20] ^= 0x04; // ein Bit im zweiten Eintrag kippen

  Gelesen gelesen;
  TEST_ASSERT_EQUAL_UINT32(0, readTable(puffer, n, sammle, &gelesen));
  TEST_ASSERT_EQUAL_UINT8(0, gelesen.anzahl);
}

/// Feinstaub und CO2 werten nur die obere Grenze aus. Ohne dieses Kennzeichen
/// zeichnete die Chronik eine rote Linie bei MHZ19_YELLOW_LOW = 400 ppm - und
/// widerspräche damit dem Status, den das Gerät selbst meldet.
void test_einseitige_grenzen_werden_mitgefuehrt() {
  uint8_t puffer[256] = {0};
  TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
  builder.addChannel(0, false, true, "MHZ19_0", "CO2", "ppm", 400.0f, 600.0f, 1200.0f, 2000.0f);
  builder.addChannel(1, true, false, "ANALOG_0", "Bodenfeuchte", "%", 10.0f, 20.0f, 80.0f, 90.0f);
  const size_t n = builder.finish();

  Gelesen gelesen;
  TEST_ASSERT_EQUAL_UINT32(n, readTable(puffer, n, sammle, &gelesen));
  TEST_ASSERT_TRUE(gelesen.eintraege[0].oneSided);
  TEST_ASSERT_FALSE(gelesen.eintraege[0].analog);
  TEST_ASSERT_FALSE(gelesen.eintraege[1].oneSided);
  TEST_ASSERT_TRUE(gelesen.eintraege[1].analog);

  // Die beiden Kennzeichen dürfen sich nicht ins Gehege kommen
  TableBuilder beides(puffer, sizeof(puffer), 1756612345UL);
  beides.addChannel(0, true, true, "X_0", "Beides", "%", 1.0f, 2.0f, 3.0f, 4.0f);
  const size_t m = beides.finish();
  Gelesen zwei;
  TEST_ASSERT_EQUAL_UINT32(m, readTable(puffer, m, sammle, &zwei));
  TEST_ASSERT_TRUE(zwei.eintraege[0].analog);
  TEST_ASSERT_TRUE(zwei.eintraege[0].oneSided);
}

/// Bei vielen angeschlossenen Sensoren passt die Tabelle nicht in einen
/// Rahmen. Sie muss sich dann aufteilen lassen, ohne dass ein Kanal verloren
/// geht - sonst stünden im Diagramm nackte Kanalnummern statt Namen.
void test_kanaltabelle_ueber_mehrere_rahmen() {
  // Sechzehn Kanäle mit realistischen Namen sprengen jeden einzelnen Rahmen.
  const char* namen[MAX_CHANNELS] = {"Lichtstaerke",   "Bodenfeuchte",   "Lufttemperatur",
                                     "Luftfeuchte",    "DS18B20_1",      "DS18B20_2",
                                     "PM10",           "PM2.5",          "CO2",
                                     "Gewicht",        "Temperatur",     "Luftdruck",
                                     "Zusatzkanal_13", "Zusatzkanal_14", "Zusatzkanal_15",
                                     "Zusatzkanal_16"};

  uint8_t strom[2048] = {0};
  size_t geschrieben = 0;
  uint8_t von = 0;
  uint8_t rahmenzahl = 0;

  // Genau die Schleife, die ChronikStore::writeTableFrames() fährt
  while (von < MAX_CHANNELS && rahmenzahl < 10) {
    uint8_t puffer[MAX_FRAME_SIZE] = {0};
    TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
    uint8_t naechster = von;
    for (uint8_t k = von; k < MAX_CHANNELS; k++) {
      char key[16];
      snprintf(key, sizeof(key), "SENSOR_%u", static_cast<unsigned>(k));
      if (!builder.addChannel(k, k < 2, false, key, namen[k],
                              "\xC2\xB5"
                              "g/m\xC2\xB3",
                              10.0f, 20.0f, 80.0f, 90.0f)) {
        break; // passt nicht mehr - nächster Rahmen
      }
      naechster = static_cast<uint8_t>(k + 1);
    }
    const size_t n = builder.finish();
    TEST_ASSERT_GREATER_THAN_UINT32(0, n);
    TEST_ASSERT_GREATER_THAN_UINT8(von, naechster); // es muss vorangehen
    memcpy(strom + geschrieben, puffer, n);
    geschrieben += n;
    von = naechster;
    rahmenzahl++;
  }

  TEST_ASSERT_GREATER_THAN_UINT8(1, rahmenzahl); // sonst prüft der Test nichts
  TEST_ASSERT_EQUAL_UINT8(MAX_CHANNELS, von);

  // Alle Rahmen hintereinander gelesen ergeben wieder alle sechzehn Kanäle
  Gelesen gelesen;
  size_t at = 0;
  while (at < geschrieben) {
    const size_t n = readTable(strom + at, geschrieben - at, sammle, &gelesen);
    TEST_ASSERT_GREATER_THAN_UINT32(0, n);
    at += n;
  }
  TEST_ASSERT_EQUAL_UINT8(MAX_CHANNELS, gelesen.anzahl);
  TEST_ASSERT_EQUAL_STRING("Lichtstaerke", gelesen.texte[0][1]);
  TEST_ASSERT_EQUAL_STRING("Zusatzkanal_16", gelesen.texte[15][1]);
  TEST_ASSERT_EQUAL_UINT8(15, gelesen.eintraege[15].channel);
}

/// Messwertnamen sind im Adminbereich frei änderbar. Wird bei 31 Byte glatt
/// abgeschnitten, kann das eine UTF-8-Folge zerteilen - im Browser stünden
/// dann kaputte Zeichen.
void test_kuerzung_zerschneidet_kein_utf8_zeichen() {
  // 30 ASCII-Zeichen, dann ein Umlaut: der Schnitt bei 31 Byte träfe mitten
  // in die Zweibytefolge.
  const char* lang = "123456789012345678901234567890\xC3\xA4rger";

  uint8_t puffer[256] = {0};
  TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
  TEST_ASSERT_TRUE(builder.addChannel(0, false, false, "K", lang, "%", 0, 0, 0, 0));
  const size_t n = builder.finish();
  TEST_ASSERT_GREATER_THAN_UINT32(0, n);

  Gelesen gelesen;
  TEST_ASSERT_EQUAL_UINT32(n, readTable(puffer, n, sammle, &gelesen));
  const char* name = gelesen.texte[0][1];
  TEST_ASSERT_EQUAL_UINT32(30, strlen(name)); // nur die 30 vollständigen Zeichen
  TEST_ASSERT_EQUAL_STRING("123456789012345678901234567890", name);

  // Passt der Text vollständig, wird nichts abgeschnitten
  TableBuilder zweiter(puffer, sizeof(puffer), 1756612345UL);
  zweiter.addChannel(0, false, false, "K",
                     "\xC2\xB5"
                     "g/m\xC2\xB3",
                     "%", 0, 0, 0, 0);
  const size_t m = zweiter.finish();
  Gelesen ganz;
  TEST_ASSERT_EQUAL_UINT32(m, readTable(puffer, m, sammle, &ganz));
  TEST_ASSERT_EQUAL_STRING("\xC2\xB5"
                           "g/m\xC2\xB3",
                           ganz.texte[0][1]);
}

/// Werte der übrigen Sensoren müssen durch das half-Format passen: CO2 bis
/// 5000 ppm, Gewicht bis 10 kg, Luftdruck um 1013 hPa, Feinstaub in µg/m³.
void test_wertebereiche_der_uebrigen_sensoren() {
  const float werte[] = {5000.0f, 2000.0f, 10000.0f, 1013.25f, 35.0f, 0.5f, -20.0f};
  const float toleranzen[] = {1.0f, 1.0f, 8.0f, 0.5f, 0.05f, 0.001f, 0.02f};
  for (size_t i = 0; i < sizeof(werte) / sizeof(werte[0]); i++) {
    const float zurueck = floatFromHalf(halfFromFloat(werte[i]));
    TEST_ASSERT_FLOAT_WITHIN(toleranzen[i], werte[i], zurueck);
  }

  // Ein voller Messrahmen mit sechzehn Kanälen bleibt weit unter der
  // Rahmengrenze - sonst ginge er beim Schreiben verloren.
  SampleFrame frame;
  frame.epoch = 1756612345UL;
  frame.count = MAX_CHANNELS;
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
    frame.values[i] = {i, STATUS_GREEN, 1013.25f, true, 512};
  }
  TEST_ASSERT_LESS_THAN_UINT32(MAX_FRAME_SIZE, sampleFrameSize(frame));

  uint8_t puffer[MAX_FRAME_SIZE] = {0};
  const size_t n = writeSample(frame, puffer, sizeof(puffer));
  SampleFrame zurueck;
  TEST_ASSERT_EQUAL_UINT32(n, readSample(puffer, n, zurueck));
  TEST_ASSERT_EQUAL_UINT8(MAX_CHANNELS, zurueck.count);
}

/// Reicht der Platz nicht, darf finish() keinen Rahmen ausgeben.
void test_kanaltabelle_ohne_platz() {
  uint8_t puffer[24] = {0};
  TableBuilder builder(puffer, sizeof(puffer), 1756612345UL);
  builder.addChannel(0, true, false, "ANALOG_0", "Lichtstaerke", "%", 10.0f, 20.0f, 80.0f, 90.0f);
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
  RUN_TEST(test_einseitige_grenzen_werden_mitgefuehrt);
  RUN_TEST(test_kanaltabelle_ueber_mehrere_rahmen);
  RUN_TEST(test_kuerzung_zerschneidet_kein_utf8_zeichen);
  RUN_TEST(test_wertebereiche_der_uebrigen_sensoren);
  RUN_TEST(test_zu_kleiner_puffer_schreibt_nichts);
  RUN_TEST(test_jedes_praefix_wird_abgelehnt);
  RUN_TEST(test_bitfehler_werden_erkannt);
  RUN_TEST(test_falsches_magic_wird_abgelehnt);
  RUN_TEST(test_findet_magic_nach_muell);
  RUN_TEST(test_gueltige_laenge_endet_vor_dem_bruch);
  return UNITY_END();
}
