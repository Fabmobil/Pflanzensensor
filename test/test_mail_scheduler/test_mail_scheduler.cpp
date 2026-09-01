/**
 * @file test_mail_scheduler.cpp
 * @brief Tests für die Auslöselogik des Mailversands (utils/mail_scheduler.h)
 *
 * Am Gerät wären diese Fälle kaum prüfbar: eine Warnsperre von vier Stunden
 * und ein Lebenszeichen alle 24 Stunden bedeuteten einen Tag pro Versuch. Und
 * genau hier entscheidet sich, ob aus einer trockenen Pflanze eine Mail wird
 * oder eine Mailflut.
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/mail_scheduler.h"

using namespace Mail;

namespace {

constexpr uint32_t T0 = 1788180000UL; // irgendein plausibler Zeitpunkt
constexpr uint32_t STUNDE = 3600UL;

SchedulerConfig standard() {
  SchedulerConfig c;
  c.enabled = true;
  c.bootMail = false;
  c.aliveMail = false;
  c.warnIntervalSeconds = 4 * STUNDE;
  c.aliveIntervalSeconds = 24 * STUNDE;
  c.settleSeconds = 180;
  return c;
}

} // namespace

/// Abgeschaltet heißt abgeschaltet - auch wenn alles brennt.
void test_ausgeschaltet_sendet_nie() {
  SchedulerConfig c = standard();
  c.enabled = false;
  c.bootMail = true;
  c.aliveMail = true;

  Scheduler s;
  s.begin(c, T0);
  s.reportLevel(0, Level::Red, true);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 10 * STUNDE));
}

/// Ohne gestellte Uhr wird nicht gesendet: das Datum der Mail wäre falsch und
/// alle Sperren rechneten mit Unsinn.
void test_ohne_uhrzeit_wird_nicht_gesendet() {
  Scheduler s;
  SchedulerConfig c = standard();
  c.bootMail = true;
  s.begin(c, T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(0));
  TEST_ASSERT_EQUAL(Kind::None, s.due(12345));
}

void test_startmeldung_genau_einmal() {
  SchedulerConfig c = standard();
  c.bootMail = true;

  Scheduler s;
  s.begin(c, T0);
  s.setSensorsReady(true);
  TEST_ASSERT_EQUAL(Kind::Boot, s.due(T0));

  s.markSent(Kind::Boot, T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 60));
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 100 * STUNDE));
}

void test_startmeldung_abgeschaltet() {
  Scheduler s;
  s.begin(standard(), T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0));
}

/// Direkt nach dem Start stehen die Sensoren auf "unbekannt" oder wärmen auf.
/// Eine Warnung daraus wäre ein Fehlalarm bei jedem Neustart.
void test_keine_warnung_in_der_beruhigungszeit() {
  Scheduler s;
  s.begin(standard(), T0);
  s.reportLevel(0, Level::Red, true);

  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 60));
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 179));
  TEST_ASSERT_EQUAL(Kind::Warning, s.due(T0 + 180));
}

void test_warnung_und_sperrfrist() {
  Scheduler s;
  s.begin(standard(), T0);
  s.reportLevel(1, Level::Yellow, true);

  const uint32_t erste = T0 + 200;
  TEST_ASSERT_EQUAL(Kind::Warning, s.due(erste));
  s.markSent(Kind::Warning, erste);

  // Innerhalb der vier Stunden bleibt es still, auch wenn der Wert rot wird
  s.reportLevel(1, Level::Red, true);
  TEST_ASSERT_EQUAL(Kind::None, s.due(erste + STUNDE));
  TEST_ASSERT_EQUAL(Kind::None, s.due(erste + 4 * STUNDE - 1));

  TEST_ASSERT_EQUAL(Kind::Warning, s.due(erste + 4 * STUNDE));
  TEST_ASSERT_EQUAL_UINT32(1, s.warningCount());
}

/// Nur ausgewählte Sensoren lösen aus - das ist der Sinn der Auswahl in der
/// Weboberfläche.
void test_nicht_ueberwachte_sensoren_loesen_nicht_aus() {
  Scheduler s;
  s.begin(standard(), T0);

  s.reportLevel(0, Level::Red, false); // rot, aber nicht überwacht
  TEST_ASSERT_FALSE(s.hasAlarm());
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + STUNDE));

  s.reportLevel(2, Level::Yellow, true);
  TEST_ASSERT_TRUE(s.hasAlarm());
  TEST_ASSERT_EQUAL(Kind::Warning, s.due(T0 + STUNDE));
}

void test_entwarnung_beendet_den_alarm() {
  Scheduler s;
  s.begin(standard(), T0);
  s.reportLevel(0, Level::Red, true);
  TEST_ASSERT_TRUE(s.hasAlarm());

  s.reportLevel(0, Level::Green, true);
  TEST_ASSERT_FALSE(s.hasAlarm());
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + STUNDE));
}

/// Das Lebenszeichen zählt ab dem Start, nicht ab null - sonst käme es bei
/// jedem Neustart sofort mit.
void test_lebenszeichen_zaehlt_ab_dem_start() {
  SchedulerConfig c = standard();
  c.aliveMail = true;

  Scheduler s;
  s.begin(c, T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + STUNDE));
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 24 * STUNDE - 1));
  TEST_ASSERT_EQUAL(Kind::Alive, s.due(T0 + 24 * STUNDE));

  s.markSent(Kind::Alive, T0 + 24 * STUNDE);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 30 * STUNDE));
  TEST_ASSERT_EQUAL(Kind::Alive, s.due(T0 + 48 * STUNDE));
}

/// Eine Warnung ist dringender als das Lebenszeichen.
void test_warnung_hat_vorrang_vor_lebenszeichen() {
  SchedulerConfig c = standard();
  c.aliveMail = true;

  Scheduler s;
  s.begin(c, T0);
  s.reportLevel(0, Level::Red, true);
  TEST_ASSERT_EQUAL(Kind::Warning, s.due(T0 + 25 * STUNDE));
}

/// Nach einem Neustart müssen die Sperren weitergelten, sonst warnt das Gerät
/// bei jedem Stromausfall sofort wieder.
void test_sperren_ueberdauern_den_neustart() {
  Scheduler s;
  s.begin(standard(), T0 + 10 * STUNDE); // Neustart zehn Stunden später
  s.restore(T0 + 9 * STUNDE, 0);         // letzte Warnung war vor einer Stunde
  s.reportLevel(0, Level::Red, true);

  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 10 * STUNDE + 200));
  TEST_ASSERT_EQUAL(Kind::Warning, s.due(T0 + 13 * STUNDE));
}

/// Stundenangaben aus der Weboberfläche: null würde jede Sperre aufheben.
void test_stunden_werden_begrenzt() {
  TEST_ASSERT_EQUAL_UINT32(4 * STUNDE, hoursToSeconds(4));
  TEST_ASSERT_EQUAL_UINT32(24 * STUNDE, hoursToSeconds(24));
  TEST_ASSERT_EQUAL_UINT32(1 * STUNDE, hoursToSeconds(0));
  TEST_ASSERT_EQUAL_UINT32(720 * STUNDE, hoursToSeconds(99999));
}

/// Kanalnummern jenseits der Grenze dürfen nichts überschreiben.
void test_kanalgrenze() {
  Scheduler s;
  s.begin(standard(), T0);
  s.reportLevel(Scheduler::MAX_CHANNELS, Level::Red, true);
  s.reportLevel(200, Level::Red, true);
  TEST_ASSERT_FALSE(s.hasAlarm());
}

/// Die Auswahl der überwachten Sensoren kommt als kommagetrennte Liste aus der
/// Weboberfläche.
void test_sensorauswahl() {
  // Leer heißt: alle überwachen
  TEST_ASSERT_TRUE(isWatched("", "ANALOG_0"));
  TEST_ASSERT_TRUE(isWatched(nullptr, "ANALOG_0"));

  TEST_ASSERT_TRUE(isWatched("ANALOG_0,DHT_1", "ANALOG_0"));
  TEST_ASSERT_TRUE(isWatched("ANALOG_0,DHT_1", "DHT_1"));
  TEST_ASSERT_FALSE(isWatched("ANALOG_0,DHT_1", "DHT_0"));

  // Der eigentliche Fallstrick: Teiltreffer dürfen nicht zählen
  TEST_ASSERT_FALSE(isWatched("DHT_10", "DHT_1"));
  TEST_ASSERT_FALSE(isWatched("ANALOG_0", "ANALOG"));
  TEST_ASSERT_TRUE(isWatched("DHT_1,DHT_10", "DHT_10"));

  // Leerzeichen um die Kommas, wie sie beim Tippen entstehen
  TEST_ASSERT_TRUE(isWatched("ANALOG_0, DHT_1", "DHT_1"));
  TEST_ASSERT_TRUE(isWatched(" ANALOG_0 , DHT_1 ", "ANALOG_0"));

  TEST_ASSERT_FALSE(isWatched("ANALOG_0", ""));
  TEST_ASSERT_FALSE(isWatched("ANALOG_0", nullptr));
}

/// Startet das Gerät ohne Netz, gibt es beim begin() noch keine Uhr. Sobald sie
/// läuft, dürfen die Fristen erst ab diesem Moment zählen - sonst käme das
/// Lebenszeichen sofort, weil "seit 1970" mehr als 24 Stunden vergangen sind.
void test_startzeitpunkt_wird_nachgetragen() {
  SchedulerConfig c = standard();
  c.aliveMail = true;

  Scheduler s;
  s.begin(c, 0); // keine Uhr beim Start
  TEST_ASSERT_EQUAL_UINT32(0, s.startedAt());

  // Ohne Nachtragen wäre jetzt sofort ein Lebenszeichen fällig
  TEST_ASSERT_EQUAL(Kind::Alive, s.due(T0));

  s.setStartedAt(T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0));
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 23 * STUNDE));
  TEST_ASSERT_EQUAL(Kind::Alive, s.due(T0 + 24 * STUNDE));
}

/// Die Startmeldung wartet auf die erste Messung aller Sensoren - sonst stünden
/// dort Lücken statt Werten
void test_startmeldung_wartet_auf_sensoren() {
  SchedulerConfig c = standard();
  c.bootMail = true;

  Scheduler s;
  s.begin(c, T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0));
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 60));

  s.setSensorsReady(true);
  TEST_ASSERT_EQUAL(Kind::Boot, s.due(T0 + 61));
}

/// Ein defekter Sensor darf die Startmeldung nicht für immer aufhalten
void test_startmeldung_hat_eine_obergrenze() {
  SchedulerConfig c = standard();
  c.bootMail = true;
  c.bootWaitSeconds = 300;

  Scheduler s;
  s.begin(c, T0);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 299));
  TEST_ASSERT_EQUAL(Kind::Boot, s.due(T0 + 300));
}

/// Ein Neustart setzt die Bereitschaft zurück - die Sensoren fangen von vorn an
void test_bereitschaft_gilt_nur_bis_zum_neustart() {
  SchedulerConfig c = standard();
  c.bootMail = true;

  Scheduler s;
  s.begin(c, T0);
  s.setSensorsReady(true);
  TEST_ASSERT_TRUE(s.sensorsReady());

  s.begin(c, T0 + 1000);
  TEST_ASSERT_FALSE(s.sensorsReady());
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 1000));
}

/// Warnschwelle Rot: gelbe Werte lösen dann keine Mail mehr aus
void test_warnschwelle_rot() {
  SchedulerConfig c = standard();
  c.warnFrom = Level::Red;

  Scheduler s;
  s.begin(c, T0);
  s.reportLevel(1, Level::Yellow, true);
  TEST_ASSERT_EQUAL(Kind::None, s.due(T0 + 200));

  s.reportLevel(1, Level::Red, true);
  TEST_ASSERT_EQUAL(Kind::Warning, s.due(T0 + 200));
}

/// Vorgabe bleibt Gelb - das ist das bisherige Verhalten
void test_warnschwelle_vorgabe_ist_gelb() {
  TEST_ASSERT_EQUAL(Level::Yellow, SchedulerConfig{}.warnFrom);

  TEST_ASSERT_TRUE(istAuffaellig(Level::Yellow, Level::Yellow));
  TEST_ASSERT_TRUE(istAuffaellig(Level::Red, Level::Yellow));
  TEST_ASSERT_FALSE(istAuffaellig(Level::Green, Level::Yellow));
  TEST_ASSERT_FALSE(istAuffaellig(Level::Yellow, Level::Red));

  // Ein Sensor ohne Messwert ist kein Alarm, obwohl Unknown numerisch kleiner
  // ist als alles andere
  TEST_ASSERT_FALSE(istAuffaellig(Level::Unknown, Level::Yellow));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_ausgeschaltet_sendet_nie);
  RUN_TEST(test_ohne_uhrzeit_wird_nicht_gesendet);
  RUN_TEST(test_startmeldung_genau_einmal);
  RUN_TEST(test_startmeldung_abgeschaltet);
  RUN_TEST(test_keine_warnung_in_der_beruhigungszeit);
  RUN_TEST(test_warnung_und_sperrfrist);
  RUN_TEST(test_nicht_ueberwachte_sensoren_loesen_nicht_aus);
  RUN_TEST(test_entwarnung_beendet_den_alarm);
  RUN_TEST(test_lebenszeichen_zaehlt_ab_dem_start);
  RUN_TEST(test_warnung_hat_vorrang_vor_lebenszeichen);
  RUN_TEST(test_sperren_ueberdauern_den_neustart);
  RUN_TEST(test_startzeitpunkt_wird_nachgetragen);
  RUN_TEST(test_sensorauswahl);
  RUN_TEST(test_stunden_werden_begrenzt);
  RUN_TEST(test_kanalgrenze);
  RUN_TEST(test_startmeldung_wartet_auf_sensoren);
  RUN_TEST(test_startmeldung_hat_eine_obergrenze);
  RUN_TEST(test_bereitschaft_gilt_nur_bis_zum_neustart);
  RUN_TEST(test_warnschwelle_rot);
  RUN_TEST(test_warnschwelle_vorgabe_ist_gelb);
  return UNITY_END();
}
