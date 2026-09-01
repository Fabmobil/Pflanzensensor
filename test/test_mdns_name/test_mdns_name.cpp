/**
 * @file test_mdns_name.cpp
 * @brief Tests für die Hostnamensableitung (utils/mdns_name.h)
 *
 * Der Gerätename ist frei wählbar, ein Hostname nicht. Geht die Umformung
 * schief, ist der Sensor unter seinem Namen einfach nicht erreichbar - ohne
 * Fehlermeldung, denn MDNS.begin() nimmt auch Unsinn entgegen.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/mdns_name.h"

using namespace MdnsName;

namespace {
/// Kurzschreibweise: umformen und das Ergebnis zurückgeben.
const char* host(const char* name) {
  static char puffer[MAX_LEN + 1];
  hostnameVon(name, puffer, sizeof(puffer));
  return puffer;
}
} // namespace

void test_leerzeichen_wird_bindestrich() {
  TEST_ASSERT_EQUAL_STRING("frameclaw-ps", host("Frameclaw PS"));
  TEST_ASSERT_EQUAL_STRING("balkon-links", host("Balkon Links"));
}

void test_grossbuchstaben_werden_klein() { TEST_ASSERT_EQUAL_STRING("sensor42", host("SENSOR42")); }

/// Ohne Ausschreiben würde aus "Grün" ein "gr-n"
void test_umlaute_werden_ausgeschrieben() {
  TEST_ASSERT_EQUAL_STRING("gruen", host("Grün"));
  TEST_ASSERT_EQUAL_STRING("aeoeue", host("äöü"));
  TEST_ASSERT_EQUAL_STRING("aeoeue", host("ÄÖÜ"));
  TEST_ASSERT_EQUAL_STRING("strasse", host("Straße"));
  TEST_ASSERT_EQUAL_STRING("mueller-balkon", host("Müller Balkon"));
}

/// Ein Emoji im Gerätenamen darf nicht vier Bindestriche ergeben
void test_sonstige_zeichen_ergeben_einen_bindestrich() {
  TEST_ASSERT_EQUAL_STRING("blume-1", host("Blume 🌻 1"));
  TEST_ASSERT_EQUAL_STRING("a-b", host("a/b"));
}

void test_bindestriche_werden_zusammengefasst() {
  TEST_ASSERT_EQUAL_STRING("a-b", host("a   b"));
  TEST_ASSERT_EQUAL_STRING("a-b", host("a---b"));
}

/// RFC 1035: nicht mit Bindestrich beginnen oder enden
void test_keine_bindestriche_am_rand() {
  TEST_ASSERT_EQUAL_STRING("mitte", host("  Mitte  "));
  TEST_ASSERT_EQUAL_STRING("mitte", host("---Mitte---"));
  TEST_ASSERT_EQUAL_STRING("mitte", host("!Mitte!"));
}

void test_leerer_name_bekommt_rueckfall() {
  TEST_ASSERT_EQUAL_STRING(RUECKFALL, host(""));
  TEST_ASSERT_EQUAL_STRING(RUECKFALL, host("   "));
  TEST_ASSERT_EQUAL_STRING(RUECKFALL, host("!!!"));
  TEST_ASSERT_EQUAL_STRING(RUECKFALL, host(nullptr));
}

/// Höchstens 63 Zeichen - und danach darf kein Bindestrich stehenbleiben
void test_laenge_wird_begrenzt() {
  char lang[200];
  memset(lang, 'a', sizeof(lang) - 1);
  lang[sizeof(lang) - 1] = '\0';
  TEST_ASSERT_EQUAL_UINT32(MAX_LEN, strlen(host(lang)));

  // Der Schnitt fällt genau auf ein Leerzeichen: der Bindestrich muss weg
  char amRand[70];
  memset(amRand, 'b', sizeof(amRand) - 1);
  amRand[sizeof(amRand) - 1] = '\0';
  amRand[MAX_LEN] = ' ';
  const char* raus = host(amRand);
  TEST_ASSERT_EQUAL_UINT32(MAX_LEN, strlen(raus));
  TEST_ASSERT_NOT_EQUAL('-', raus[strlen(raus) - 1]);
}

/// Ein zu kleiner Zielpuffer darf nicht überlaufen
void test_kleiner_puffer() {
  char klein[8];
  const size_t n = hostnameVon("Frameclaw PS", klein, sizeof(klein));
  TEST_ASSERT_EQUAL_UINT32(7, n);
  TEST_ASSERT_EQUAL_STRING("framecl", klein);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_leerzeichen_wird_bindestrich);
  RUN_TEST(test_grossbuchstaben_werden_klein);
  RUN_TEST(test_umlaute_werden_ausgeschrieben);
  RUN_TEST(test_sonstige_zeichen_ergeben_einen_bindestrich);
  RUN_TEST(test_bindestriche_werden_zusammengefasst);
  RUN_TEST(test_keine_bindestriche_am_rand);
  RUN_TEST(test_leerer_name_bekommt_rueckfall);
  RUN_TEST(test_laenge_wird_begrenzt);
  RUN_TEST(test_kleiner_puffer);
  return UNITY_END();
}
