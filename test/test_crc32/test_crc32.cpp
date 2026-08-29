/**
 * @file test_crc32.cpp
 * @brief Tests für Crc32::update()/calculate() (utils/crc32.h)
 *
 * Getestet wird die echte, unveränderte Implementierung - keine Kopie. Die
 * Referenzwerte stammen von Pythons zlib.crc32() (derselbe Algorithmus:
 * CRC-32/ISO-HDLC, identisch zu zlib/PNG/Ethernet), unabhängig vom
 * Projektcode berechnet.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/crc32.h"

namespace {
uint32_t crcOf(const char* text) {
  return Crc32::calculate(reinterpret_cast<const uint8_t*>(text), strlen(text));
}
} // namespace

/// Leere Eingabe ergibt 0 - Eigenschaft des Algorithmus (Start 0xFFFFFFFF,
/// Schluss-XOR mit 0xFFFFFFFF heben sich ohne Daten dazwischen auf).
void test_leere_eingabe_ergibt_null() {
  TEST_ASSERT_EQUAL_UINT32(0x00000000UL, Crc32::calculate(nullptr, 0));
}

/// Der klassische Prüfwert für CRC-32/ISO-HDLC, den praktisch jede
/// Implementierung als ersten Sanity-Check verwendet.
void test_standard_pruefwert_123456789() {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926UL, crcOf("123456789"));
}

/// Ein einzelnes Byte - deckt den kürzesten nichttrivialen Fall ab.
void test_einzelnes_zeichen() { TEST_ASSERT_EQUAL_HEX32(0xE8B7BE43UL, crcOf("a")); }

/// Realistischer Text, wie er beim Sichern der Konfiguration tatsächlich vorkommt.
void test_projekttext() { TEST_ASSERT_EQUAL_HEX32(0x00C18D54UL, crcOf("Pflanzensensor")); }

/// Alle 256 Bytewerte einmal - deckt jeden Tabelleneintrag mindestens einmal ab.
void test_alle_bytewerte_einmal() {
  uint8_t data[256];
  for (int i = 0; i < 256; i++) {
    data[i] = static_cast<uint8_t>(i);
  }
  TEST_ASSERT_EQUAL_HEX32(0x29058C73UL, Crc32::calculate(data, sizeof(data)));
}

/**
 * update() über mehrere Häppchen muss dasselbe Ergebnis liefern wie
 * calculate() über den zusammenhängenden Puffer. Das ist genau die
 * Eigenschaft, die flash_persistence.cpp braucht: die Prüfsumme läuft dort
 * über mehrere einzeln geschriebene Dateien fort, nicht über einen
 * einzigen Aufruf. Eine frühere Regression genau hier - die CRC beim
 * Schreiben lief über die datei-weise gepaddeten Chunks, die beim Lesen
 * berechnete aber über den zusammenhängenden Puffer - erzeugte
 * unterschiedliche Werte trotz identischer Nutzdaten.
 */
void test_fortlaufende_berechnung_entspricht_zusammenhaengendem_puffer() {
  const char* text = "Pflanzensensor Frameclaw PS";
  size_t len = strlen(text);
  uint32_t erwartet = Crc32::calculate(reinterpret_cast<const uint8_t*>(text), len);

  // Dieselben Daten in drei ungleich große Häppchen zerlegt.
  size_t split1 = 5;
  size_t split2 = 17;
  uint32_t laufend = Crc32::update(0xFFFFFFFF, reinterpret_cast<const uint8_t*>(text), split1);
  laufend =
      Crc32::update(laufend, reinterpret_cast<const uint8_t*>(text + split1), split2 - split1);
  laufend = Crc32::update(laufend, reinterpret_cast<const uint8_t*>(text + split2), len - split2);

  TEST_ASSERT_EQUAL_HEX32(erwartet, ~laufend);
}

/// Ein einziges verändertes Byte muss (mit an Sicherheit grenzender
/// Wahrscheinlichkeit) einen anderen Wert ergeben - die eigentliche Aufgabe
/// einer Prüfsumme: Datenveränderungen erkennen.
void test_erkennt_einzelnes_veraendertes_byte() {
  uint32_t original = crcOf("Fabmobil");
  uint32_t veraendert = crcOf("Fabmovil"); // b -> v
  TEST_ASSERT_NOT_EQUAL(original, veraendert);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_leere_eingabe_ergibt_null);
  RUN_TEST(test_standard_pruefwert_123456789);
  RUN_TEST(test_einzelnes_zeichen);
  RUN_TEST(test_projekttext);
  RUN_TEST(test_alle_bytewerte_einmal);
  RUN_TEST(test_fortlaufende_berechnung_entspricht_zusammenhaengendem_puffer);
  RUN_TEST(test_erkennt_einzelnes_veraendertes_byte);
  return UNITY_END();
}
