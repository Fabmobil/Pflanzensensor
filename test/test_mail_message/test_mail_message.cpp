/**
 * @file test_mail_message.cpp
 * @brief Tests für den Nachrichtenbau (utils/mail_message.h)
 *
 * Die Referenzwerte stammen aus Pythons email.utils bzw. base64 - also von
 * einer unabhängigen Umsetzung derselben Normen. Sichtbar werden Fehler hier
 * sonst erst im Postfach: ein falsches Datum sortiert die Mail ans falsche
 * Ende, ein unkodierter Umlaut im Betreff zeigt Buchstabensalat.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/mail_message.h"

using namespace Mail;

// === Datum ===

void test_datum_nach_rfc5322() {
  char out[48];

  TEST_ASSERT_GREATER_THAN_UINT32(0, formatDate(1788180876, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Mon, 31 Aug 2026 12:54:36 +0000", out);

  formatDate(1000000000, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Sun, 09 Sep 2001 01:46:40 +0000", out);

  formatDate(1767225600, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Thu, 01 Jan 2026 00:00:00 +0000", out);
}

/// Ohne gestellte Uhr lieber gar kein Datum als eines aus dem Jahr 1970 - die
/// Mail bekäme sonst beim Empfänger einen völlig falschen Platz.
void test_datum_ohne_uhrzeit() {
  char out[48];
  TEST_ASSERT_EQUAL_UINT32(0, formatDate(0, out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, formatDate(12345, out, sizeof(out)));
}

void test_datum_zu_kleiner_puffer() {
  char out[16];
  memset(out, 0x7F, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, formatDate(1788180876, out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
}

// === Betreff ===

void test_betreff_ohne_umlaute_bleibt_unveraendert() {
  char out[128];
  const char* betreff = "Pflanzensensor: Bodenfeuchte kritisch";
  TEST_ASSERT_EQUAL_UINT32(strlen(betreff), encodeSubject(betreff, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING(betreff, out);
}

void test_betreff_mit_umlauten_wird_kodiert() {
  char out[128];

  encodeSubject("Lichtstärke zu niedrig", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("=?UTF-8?B?TGljaHRzdMOkcmtlIHp1IG5pZWRyaWc=?=", out);

  encodeSubject("Bodenfeuchte kritisch (Gießkanne!)", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("=?UTF-8?B?Qm9kZW5mZXVjaHRlIGtyaXRpc2NoIChHaWXDn2thbm5lISk=?=", out);
}

void test_betreff_zu_kleiner_puffer() {
  char out[8];
  memset(out, 0x7F, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, encodeSubject("Lichtstärke zu niedrig", out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
  TEST_ASSERT_EQUAL_UINT32(0, encodeSubject("viel zu langer ASCII-Betreff", out, sizeof(out)));
}

void test_ascii_erkennung() {
  TEST_ASSERT_TRUE(isPlainAscii("Bodenfeuchte kritisch"));
  TEST_ASSERT_TRUE(isPlainAscii(""));
  TEST_ASSERT_FALSE(isPlainAscii("Lichtstärke"));
  // Ein Zeilenumbruch im Betreff wäre eine Kopfzeilen-Einschleusung
  TEST_ASSERT_FALSE(isPlainAscii("Betreff\r\nBcc: wer@anders.de"));
}

// === Message-ID und Domäne ===

void test_message_id() {
  char out[96];
  TEST_ASSERT_GREATER_THAN_UINT32(
      0, formatMessageId("Frameclaw PS", "datenkollektiv.net", 1788180876, 7, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("<1788180876.7.Frameclaw PS@datenkollektiv.net>", out);
}

void test_domaene_aus_adresse() {
  char out[64];
  TEST_ASSERT_EQUAL_UINT32(18, domainOf("pflanzensensor@datenkollektiv.net", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("datenkollektiv.net", out);

  TEST_ASSERT_EQUAL_UINT32(0, domainOf("ohne-at-zeichen", out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, domainOf("endet-mit@", out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, domainOf(nullptr, out, sizeof(out)));
}

// === Adressprüfung ===

/// Fängt Tippfehler in der Weboberfläche ab, bevor der Server mit 550 antwortet
/// und niemand weiß, warum keine Mail ankommt.
void test_adressen_pruefung() {
  TEST_ASSERT_TRUE(looksLikeAddress("gaertner@example.org"));
  TEST_ASSERT_TRUE(looksLikeAddress("pflanzensensor@datenkollektiv.net"));
  TEST_ASSERT_TRUE(looksLikeAddress("a.b+c@sub.example.co.uk"));

  TEST_ASSERT_FALSE(looksLikeAddress(""));
  TEST_ASSERT_FALSE(looksLikeAddress(nullptr));
  TEST_ASSERT_FALSE(looksLikeAddress("ohne-at.example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("@example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("wer@ohnepunkt"));
  TEST_ASSERT_FALSE(looksLikeAddress("wer@.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("zwei@at@example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("mit leerzeichen@example.org"));
  // Kopfzeilen-Einschleusung über die Empfängeradresse
  TEST_ASSERT_FALSE(looksLikeAddress("wer@example.org\r\nBcc: dritte@example.org"));
  TEST_ASSERT_FALSE(looksLikeAddress("a@b.c,d@e.f"));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_datum_nach_rfc5322);
  RUN_TEST(test_datum_ohne_uhrzeit);
  RUN_TEST(test_datum_zu_kleiner_puffer);
  RUN_TEST(test_betreff_ohne_umlaute_bleibt_unveraendert);
  RUN_TEST(test_betreff_mit_umlauten_wird_kodiert);
  RUN_TEST(test_betreff_zu_kleiner_puffer);
  RUN_TEST(test_ascii_erkennung);
  RUN_TEST(test_message_id);
  RUN_TEST(test_domaene_aus_adresse);
  RUN_TEST(test_adressen_pruefung);
  return UNITY_END();
}
