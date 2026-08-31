/**
 * @file test_smtp_session.cpp
 * @brief Tests für den SMTP-Ablauf (utils/smtp_session.h)
 *
 * Der Versand läuft höchstens ein paar Mal am Tag und dann gegen einen echten
 * Mailserver - Fehler fallen also frühestens Stunden später auf, wenn eine
 * Warnung ausbleibt. Deshalb wird der Ablauf hier vollständig durchgespielt,
 * inklusive der Stellen, die man beim Ausprobieren nur zufällig trifft:
 * mehrzeilige Antworten, 4xx gegen 5xx, Punkt-Stuffing.
 *
 * Die Base64-Referenzwerte stammen aus RFC 4648 bzw. sind mit Pythons
 * base64-Modul unabhängig nachgerechnet.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/smtp_session.h"

using namespace Smtp;

namespace {

Config testConfig(bool starttls = false) {
  Config c;
  c.helo = "pflanzensensor";
  c.user = "pflanzensensor@datenkollektiv.net";
  c.password = "geheim";
  c.from = "pflanzensensor@datenkollektiv.net";
  c.to = "gaertner@example.org";
  c.useStartTls = starttls;
  return c;
}

/// Bequemer Durchlauf: Zeile füttern und den nächsten Schritt zurückgeben.
Step fuettere(Session& s, const char* line) { return s.feedLine(line); }

} // namespace

// === Base64 ===

void test_base64_gegen_rfc4648() {
  char out[64];
  struct {
    const char* ein;
    const char* aus;
  } proben[] = {{"", ""},
                {"f", "Zg=="},
                {"fo", "Zm8="},
                {"foo", "Zm9v"},
                {"foob", "Zm9vYg=="},
                {"fooba", "Zm9vYmE="},
                {"foobar", "Zm9vYmFy"}};
  for (size_t i = 0; i < sizeof(proben) / sizeof(proben[0]); i++) {
    base64Encode(proben[i].ein, strlen(proben[i].ein), out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(proben[i].aus, out);
  }
}

void test_base64_echte_zugangsdaten() {
  char out[80];
  base64Encode("pflanzensensor", 14, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("cGZsYW56ZW5zZW5zb3I=", out);
}

/// Zu kleiner Puffer darf nichts schreiben, sonst ginge ein halbes Passwort
/// über die Leitung.
void test_base64_zu_kleiner_puffer() {
  char out[4];
  memset(out, 0x7F, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, base64Encode("foobar", 6, out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
}

// === Antwortzeilen ===

void test_antwortcode_lesen() {
  TEST_ASSERT_EQUAL_INT(220, replyCode("220 mx1.datenkollektiv.net ESMTP Postfix"));
  TEST_ASSERT_EQUAL_INT(250, replyCode("250-STARTTLS"));
  TEST_ASSERT_EQUAL_INT(354, replyCode("354 End data with <CR><LF>.<CR><LF>"));
  TEST_ASSERT_EQUAL_INT(0, replyCode("Zwischenzeile ohne Code"));
  TEST_ASSERT_EQUAL_INT(0, replyCode("25"));
  TEST_ASSERT_EQUAL_INT(0, replyCode(nullptr));
}

/// Der Bindestrich nach dem Code ist der ganze Unterschied zwischen "es kommt
/// noch mehr" und "fertig" - wer ihn übersieht, sendet mitten in die Antwort.
void test_mehrzeilige_antwort_erkennen() {
  TEST_ASSERT_FALSE(isFinalLine("250-mx1.datenkollektiv.net"));
  TEST_ASSERT_FALSE(isFinalLine("250-AUTH PLAIN LOGIN"));
  TEST_ASSERT_TRUE(isFinalLine("250 STARTTLS"));
  TEST_ASSERT_TRUE(isFinalLine("220 bereit"));
}

// === Ablauf ===

/// Der volle Weg über Port 465: TLS steht schon, es wird angemeldet.
void test_ablauf_mit_anmeldung() {
  Session s;
  s.begin(testConfig(false));

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "220 mx1.datenkollektiv.net ESMTP Postfix"));
  TEST_ASSERT_EQUAL_STRING("EHLO pflanzensensor", s.command());

  // Mehrzeilige EHLO-Antwort: erst die letzte Zeile treibt weiter
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "250-mx1.datenkollektiv.net"));
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "250-PIPELINING"));
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "250-AUTH PLAIN LOGIN"));
  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 8BITMIME"));
  TEST_ASSERT_TRUE(s.authOffered());
  TEST_ASSERT_EQUAL_STRING("AUTH LOGIN", s.command());

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "334 VXNlcm5hbWU6"));
  TEST_ASSERT_EQUAL_STRING("cGZsYW56ZW5zZW5zb3JAZGF0ZW5rb2xsZWt0aXYubmV0", s.command());

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "334 UGFzc3dvcmQ6"));
  TEST_ASSERT_EQUAL_STRING("Z2VoZWlt", s.command()); // "geheim"

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "235 2.7.0 Authentication successful"));
  TEST_ASSERT_EQUAL_STRING("MAIL FROM:<pflanzensensor@datenkollektiv.net>", s.command());

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 2.1.0 Ok"));
  TEST_ASSERT_EQUAL_STRING("RCPT TO:<gaertner@example.org>", s.command());

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 2.1.5 Ok"));
  TEST_ASSERT_EQUAL_STRING("DATA", s.command());

  TEST_ASSERT_EQUAL(Step::SendBody, fuettere(s, "354 End data with <CR><LF>.<CR><LF>"));

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 2.0.0 Ok: queued as 4XYZ"));
  TEST_ASSERT_EQUAL_STRING("QUIT", s.command());

  TEST_ASSERT_EQUAL(Step::Done, fuettere(s, "221 2.0.0 Bye"));
  TEST_ASSERT_EQUAL(Phase::Done, s.phase());
}

/// Port 587: unverschlüsselt begonnen, per STARTTLS gesichert. Danach muss
/// EHLO wiederholt werden.
void test_ablauf_mit_starttls() {
  Session s;
  s.begin(testConfig(true));

  fuettere(s, "220 mail.example.org ESMTP");
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "250-mail.example.org"));
  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 STARTTLS"));
  TEST_ASSERT_EQUAL_STRING("STARTTLS", s.command());

  // Der Aufrufer soll jetzt TLS aushandeln und danach das EHLO senden
  TEST_ASSERT_EQUAL(Step::StartTls, fuettere(s, "220 2.0.0 Ready to start TLS"));
  TEST_ASSERT_EQUAL_STRING("EHLO pflanzensensor", s.command());

  // Vor TLS gesehene Erweiterungen zählen nicht mehr
  TEST_ASSERT_FALSE(s.authOffered());
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "250-mail.example.org"));
  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 AUTH LOGIN PLAIN"));
  TEST_ASSERT_TRUE(s.authOffered());
  TEST_ASSERT_EQUAL_STRING("AUTH LOGIN", s.command());
}

/// Ohne Zugangsdaten wird die Anmeldung übersprungen.
void test_ablauf_ohne_anmeldung() {
  Config c = testConfig(false);
  c.user = nullptr;
  c.password = nullptr;

  Session s;
  s.begin(c);
  fuettere(s, "220 bereit");
  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 fertig"));
  TEST_ASSERT_EQUAL_STRING("MAIL FROM:<pflanzensensor@datenkollektiv.net>", s.command());
}

/// Falsches Passwort: 535 ist endgültig, ein erneuter Versuch wäre sinnlos.
void test_anmeldung_abgelehnt() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  fuettere(s, "250 AUTH LOGIN");
  fuettere(s, "334 VXNlcm5hbWU6");
  fuettere(s, "334 UGFzc3dvcmQ6");

  TEST_ASSERT_EQUAL(Step::Failed, fuettere(s, "535 5.7.8 Authentication credentials invalid"));
  TEST_ASSERT_EQUAL(Phase::Failed, s.phase());
  TEST_ASSERT_EQUAL_INT(535, s.lastCode());
  // Der Code steht im Fehlertext, damit man 4xx von 5xx unterscheiden kann
  TEST_ASSERT_NOT_NULL(strstr(s.error(), "535"));
}

/// Unbekannter Empfänger - der Server nimmt die Nachricht gar nicht erst an.
void test_empfaenger_abgelehnt() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  fuettere(s, "250 AUTH LOGIN");
  fuettere(s, "334 VXNlcm5hbWU6");
  fuettere(s, "334 UGFzc3dvcmQ6");
  fuettere(s, "235 ok");
  fuettere(s, "250 ok");

  TEST_ASSERT_EQUAL(Step::Failed, fuettere(s, "550 5.1.1 User unknown"));
  TEST_ASSERT_NOT_NULL(strstr(s.error(), "RCPT TO"));
}

/// Verlangt der Server eine Anmeldung, die er nicht anbietet, bricht die
/// Sitzung ab, statt das Passwort ins Leere zu senden.
void test_server_ohne_auth_mit_zugangsdaten() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  TEST_ASSERT_EQUAL(Step::Failed, fuettere(s, "250 PIPELINING"));
  TEST_ASSERT_NOT_NULL(strstr(s.error(), "keine Anmeldung"));
}

/// Ein voller Briefkasten meldet sich mit 4xx - das ist vorübergehend und
/// rechtfertigt einen späteren Versuch.
void test_voruebergehender_fehler() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  fuettere(s, "250 AUTH LOGIN");
  fuettere(s, "334 VXNlcm5hbWU6");
  fuettere(s, "334 UGFzc3dvcmQ6");
  fuettere(s, "235 ok");

  TEST_ASSERT_EQUAL(Step::Failed, fuettere(s, "452 4.3.1 Insufficient system storage"));
  TEST_ASSERT_EQUAL_INT(452, s.lastCode());
}

/// Zwischenzeilen ohne Antwortcode dürfen den Ablauf nicht durcheinanderbringen.
void test_zeilen_ohne_code_werden_uebergangen() {
  Session s;
  s.begin(testConfig(false));
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, ""));
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "irgendwas"));
  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "220 bereit"));
}

/// Bietet der Server nur PLAIN an, muss das benutzt werden - sonst endet der
/// Versuch mit demselben 535 wie ein falsches Passwort, und niemand käme auf
/// die Ursache.
void test_auth_plain_wenn_login_fehlt() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  TEST_ASSERT_EQUAL(Step::Wait, fuettere(s, "250-mail.example.org"));
  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "250 AUTH PLAIN"));

  TEST_ASSERT_TRUE(s.supportsAuthPlain());
  TEST_ASSERT_FALSE(s.supportsAuthLogin());
  // \0benutzer\0passwort, base64-kodiert (Referenz aus Pythons base64)
  TEST_ASSERT_EQUAL_STRING("AUTH PLAIN AHBmbGFuemVuc2Vuc29yQGRhdGVua29sbGVrdGl2Lm5ldABnZWhlaW0=",
                           s.command());

  TEST_ASSERT_EQUAL(Step::Send, fuettere(s, "235 2.7.0 Authentication successful"));
  TEST_ASSERT_EQUAL_STRING("MAIL FROM:<pflanzensensor@datenkollektiv.net>", s.command());
}

/// Bietet der Server beides an, bleibt es bei LOGIN - das ist der verbreitetere
/// Weg und bereits erprobt.
void test_login_hat_vorrang_wenn_beides_angeboten_wird() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  fuettere(s, "250 AUTH PLAIN LOGIN");
  TEST_ASSERT_TRUE(s.supportsAuthLogin());
  TEST_ASSERT_TRUE(s.supportsAuthPlain());
  TEST_ASSERT_EQUAL_STRING("AUTH LOGIN", s.command());
}

/// Bei einer abgelehnten Anmeldung ist der Klartext des Servers das einzige,
/// was weiterhilft - "authentication failed" liest sich anders als
/// "Invalid user".
void test_fehlertext_enthaelt_die_serverantwort() {
  Session s;
  s.begin(testConfig(false));
  fuettere(s, "220 bereit");
  fuettere(s, "250 AUTH LOGIN");
  fuettere(s, "334 VXNlcm5hbWU6");
  fuettere(s, "334 UGFzc3dvcmQ6");
  fuettere(s, "535 5.7.8 Error: authentication failed: generic failure");

  TEST_ASSERT_EQUAL_INT(535, s.lastCode());
  TEST_ASSERT_EQUAL_STRING("535 5.7.8 Error: authentication failed: generic failure", s.lastText());
  TEST_ASSERT_NOT_NULL(strstr(s.error(), "authentication failed"));
  TEST_ASSERT_NOT_NULL(strstr(s.error(), "Anmeldung"));
}

// === Punkt-Stuffing ===

void test_punkt_stuffing() {
  char out[64];

  // Rückgabe ist die Länge der AUSGABE, also einschließlich des gestopften Punkts
  TEST_ASSERT_EQUAL_UINT32(7, stuffLine(".hallo", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("..hallo", out);

  TEST_ASSERT_EQUAL_UINT32(2, stuffLine(".", out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("..", out);

  // Ein Punkt mitten in der Zeile ist harmlos
  stuffLine("Wert 23.5 Grad", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Wert 23.5 Grad", out);

  stuffLine("", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_punkt_stuffing_zu_kleiner_puffer() {
  char out[4];
  memset(out, 0x7F, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(0, stuffLine(".abcd", out, sizeof(out)));
  TEST_ASSERT_EQUAL_HEX8(0x7F, out[0]);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_base64_gegen_rfc4648);
  RUN_TEST(test_base64_echte_zugangsdaten);
  RUN_TEST(test_base64_zu_kleiner_puffer);
  RUN_TEST(test_antwortcode_lesen);
  RUN_TEST(test_mehrzeilige_antwort_erkennen);
  RUN_TEST(test_ablauf_mit_anmeldung);
  RUN_TEST(test_ablauf_mit_starttls);
  RUN_TEST(test_ablauf_ohne_anmeldung);
  RUN_TEST(test_anmeldung_abgelehnt);
  RUN_TEST(test_empfaenger_abgelehnt);
  RUN_TEST(test_server_ohne_auth_mit_zugangsdaten);
  RUN_TEST(test_voruebergehender_fehler);
  RUN_TEST(test_zeilen_ohne_code_werden_uebergangen);
  RUN_TEST(test_auth_plain_wenn_login_fehlt);
  RUN_TEST(test_login_hat_vorrang_wenn_beides_angeboten_wird);
  RUN_TEST(test_fehlertext_enthaelt_die_serverantwort);
  RUN_TEST(test_punkt_stuffing);
  RUN_TEST(test_punkt_stuffing_zu_kleiner_puffer);
  return UNITY_END();
}
