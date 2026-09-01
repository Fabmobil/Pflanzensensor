/**
 * @file mail_sender.cpp
 * @brief Umsetzung des Mailversands (siehe mail_sender.h)
 */

#include "mail/mail_sender.h"

#include "mail/mail_client.h"
#include "mail/mail_vorlagen.h"
#include "utils/mail_template.h"

#include <memory>
#include <new>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/helper.h"
#include "utils/memory_manager.h"

extern std::unique_ptr<SensorManager> sensorManager;

namespace {

/// Alle 30 Sekunden prüfen, ob etwas ansteht. Häufiger wäre sinnlos: die
/// kürzeste Sperre ist eine Stunde.
constexpr uint32_t PRUEFINTERVALL_MS = 30000;

Mail::Level levelVon(const String& status) { return Mail::levelVonText(status.c_str()); }

} // namespace

MailSender& MailSender::instance() {
  static MailSender sender;
  return sender;
}

void MailSender::begin() {
  reloadConfig();
  m_scheduler.restore(ConfigMgr.getMailLastWarning(), ConfigMgr.getMailLastAlive());
  if (!ConfigMgr.isMailBootEnabled()) {
    m_scheduler.skipBootMail();
  }
  LOG_INFO(F("Mail"), ConfigMgr.isMailEnabled() ? F("Mailversand aktiv") : F("Mailversand aus"));
}

void MailSender::reloadConfig() {
  Mail::SchedulerConfig c;
  c.enabled = ConfigMgr.isMailEnabled();
  c.bootMail = ConfigMgr.isMailBootEnabled();
  c.aliveMail = ConfigMgr.isMailAliveEnabled();
  c.warnIntervalSeconds = Mail::hoursToSeconds(ConfigMgr.getMailWarnHours());
  c.warnFrom = ConfigMgr.getMailWarnFrom() == 2 ? Mail::Level::Red : Mail::Level::Yellow;
  c.aliveIntervalSeconds = Mail::hoursToSeconds(ConfigMgr.getMailAliveHours());

  const uint32_t jetzt = static_cast<uint32_t>(Helper::getCurrentTime());
  if (m_lastAttempt == 0 && m_sent == 0) {
    m_scheduler.begin(c, jetzt);
    m_scheduler.restore(ConfigMgr.getMailLastWarning(), ConfigMgr.getMailLastAlive());
  } else {
    m_scheduler.setConfig(c);
  }
}

void MailSender::requestTestMail() {
  m_testPending = true;
  // Zwei Minuten Geduld bei Speichermangel. Der Webserver gibt seine Puffer
  // gleich nach der Antwort frei, und der Aufruf kommt ja gerade von dort -
  // ein Fehlschlag um ein paar hundert Byte wäre nur eine Frage des
  // Zeitpunkts, nicht der Einstellungen.
  m_testFristMs = millis() + TEST_GEDULD_MS;
}

void MailSender::aktualisiereZustaende() {
  if (!sensorManager) {
    return;
  }
  uint8_t kanal = 0;
  bool alleGemessen = true;
  for (const auto& sensor : sensorManager->getSensors()) {
    if (!sensor || !sensor->isEnabled()) {
      continue;
    }
    // Dieselbe Bedingung wie im SensorHandler (sensor_handler.cpp:231): ohne
    // Startzeit hat der Sensor noch nie erfolgreich gemessen, seine Werte wären
    // die Platzhalter aus dem Hochlauf.
    if (sensor->getMeasurementStartTime() == 0) {
      alleGemessen = false;
    }
    const SensorConfig& config = sensor->config();
    for (size_t i = 0; i < config.activeMeasurements; i++) {
      if (!config.measurements[i].enabled) {
        continue;
      }
      if (kanal >= Mail::Scheduler::MAX_CHANNELS) {
        break; // kein return: sonst bliebe die Bereitschaft unten ungemeldet
      }
      const String schluessel = sensor->getId() + "_" + String(i);
      m_scheduler.reportLevel(kanal, levelVon(sensor->getStatus(i)),
                              ConfigMgr.isMailSensorWatched(schluessel));
      kanal++;
    }
  }
  m_scheduler.setSensorsReady(alleGemessen);
}

void MailSender::loop() {
  const uint32_t jetztMs = millis();
  // Eine angeforderte Testmail hat es eilig, darf die Schleife aber nicht mit
  // voller Geschwindigkeit belegen: sie kann auf freien Speicher warten müssen.
  // Die zwei Sekunden Verzug schaden nicht, im Gegenteil - der Webserver gibt
  // in der Zeit die Puffer der Antwort frei, die den Test ausgelöst hat.
  const uint32_t abstand = m_testPending ? TEST_ABSTAND_MS : PRUEFINTERVALL_MS;
  if ((jetztMs - m_lastCheck) < abstand) {
    return;
  }
  m_lastCheck = jetztMs;

  if (!ConfigMgr.isMailEnabled()) {
    m_testPending = false;
    return;
  }

  aktualisiereZustaende();

  const uint32_t jetzt = static_cast<uint32_t>(Helper::getCurrentTime());

  // Beim Start war womöglich noch keine Uhr da (NTP braucht ein paar Sekunden,
  // nach einem Neustart ohne Netz auch mal Minuten). Dann steht der
  // Startzeitpunkt des Planers auf null, und sobald die Uhr läuft, wäre das
  // Lebenszeichen "seit Jahrzehnten fällig" - es käme sofort.
  if (jetzt > 1600000000UL && m_scheduler.startedAt() < 1600000000UL) {
    m_scheduler.setStartedAt(jetzt);
    LOG_DEBUG(F("Mail"), F("Startzeitpunkt nachgetragen, nachdem die Uhr lief"));
  }
  Mail::Kind was = Mail::Kind::None;
  if (m_testPending) {
    was = Mail::Kind::Alive; // die Testmail ist inhaltlich ein Lebenszeichen
  } else {
    was = m_scheduler.due(jetzt);
  }

  // Warum passiert (nichts)? Ohne diese Zeile bleibt die Entscheidung des
  // Zeitplaners von außen unsichtbar - man sieht nur, dass keine Mail kommt.
  if (ConfigMgr.isDebugMail() && was == Mail::Kind::None) {
    LOG_DEBUG(F("Mail"), String(F("nichts faellig: Alarm=")) + (m_scheduler.hasAlarm() ? 1 : 0) +
                             F(" Sensoren bereit=") + (m_scheduler.sensorsReady() ? 1 : 0) +
                             F(" seit Start=") + (jetzt - m_scheduler.startedAt()) + F("s") +
                             F(" letzteWarnung=") + m_scheduler.lastWarning() +
                             F(" letztesLebenszeichen=") + m_scheduler.lastAlive());
  }
  if (was == Mail::Kind::None) {
    return;
  }

  String fehler;
  const bool istTest = m_testPending;
  m_testPending = false;
  m_lastAttempt = jetzt;

  if (sende(was, fehler)) {
    m_sent++;
    m_lastResult = F("Versand erfolgreich");
    LOG_INFO(F("Mail"), F("Mail verschickt"));
    if (!istTest) {
      m_scheduler.markSent(was, jetzt);
      // Zeitpunkte festhalten, damit die Sperren einen Neustart überdauern
      if (was == Mail::Kind::Warning) {
        ConfigMgr.setMailLastWarning(jetzt);
      } else if (was == Mail::Kind::Alive) {
        ConfigMgr.setMailLastAlive(jetzt);
      }
    }
  } else if (istTest && m_voruebergehend && static_cast<int32_t>(m_testFristMs - jetztMs) > 0) {
    // Der Speicher ist ein Momentzustand, keine falsche Einstellung: gleich
    // noch einmal versuchen. Wichtig ist, dass der Versuch überhaupt
    // stattfindet - erst er räumt den Handler-Cache des Webservers leer, und
    // das sind die entscheidenden Kilobyte. Eine Vorabprüfung an dieser Stelle
    // hätte genau das verhindert und die Testmail dauerhaft blockiert.
    m_testPending = true;
    m_lastResult = String(F("Wird gleich noch einmal versucht - ")) + fehler;
    LOG_DEBUG(F("Mail"), m_lastResult);
  } else {
    m_lastResult = fehler;
    LOG_ERROR(F("Mail"), String(F("Versand fehlgeschlagen: ")) + fehler);
    // Auch ein Fehlschlag setzt die Sperre: sonst versucht das Gerät es bei
    // falschem Passwort alle 30 Sekunden erneut und der Server sperrt uns aus.
    if (!istTest && was != Mail::Kind::Boot) {
      m_scheduler.markSent(was, jetzt);
    } else if (!istTest) {
      m_scheduler.skipBootMail();
    }
  }
}

namespace {

/**
 * @brief Sieht das nach einer Mailadresse aus?
 * @details Absichtlich grob: genau ein @, davor und danach etwas, hinten ein
 *          Punkt. Die verbindliche Prüfung macht der Mailbot
 *          (Validation::isValidEmail); hier geht es nur darum, einen leeren
 *          oder offensichtlich vertippten Eintrag zu melden, bevor eine Mail
 *          verschlüsselt und verschickt wird.
 */
bool sinnvolleAdresse(const String& adresse) {
  const int at = adresse.indexOf('@');
  if (at <= 0 || at != adresse.lastIndexOf('@')) {
    return false;
  }
  const int punkt = adresse.indexOf('.', at + 2);
  return punkt > 0 && punkt < static_cast<int>(adresse.length()) - 1;
}

/// Senke, die die gerenderten Zeilen an einen String anhängt statt an einen
/// Socket. Der Rumpf muss diesmal am Stück vorliegen: er wird als Ganzes
/// verschlüsselt. Bei höchstens 2000 Byte ist das unkritisch - der alte Weg
/// brauchte allein für den TLS-Handshake 11,7 KB.
void haengeAn(const char* text, size_t length, void* context) {
  String* ziel = static_cast<String*>(context);
  ziel->concat(text, length);
  ziel->concat('\n');
  optimistic_yield(1000);
}

} // namespace

bool MailSender::sende(Mail::Kind kind, String& fehler) {
  const String an = ConfigMgr.getMailTo();
  m_voruebergehend = false;
  if (!sinnvolleAdresse(an)) {
    fehler = F("Empfaengeradresse fehlt oder ist unbrauchbar");
    return false;
  }
  if (ConfigMgr.getMailServiceUrl().length() == 0 || ConfigMgr.getMailDeviceId().length() == 0 ||
      ConfigMgr.getMailSecretKey().length() == 0) {
    fehler = F("Mailbot-Zugang unvollstaendig (Adresse, Geraete-ID oder Schluessel fehlt)");
    return false;
  }

  // Der Webserver hält seine zuletzt benutzten Handler im Cache. Sie kosten
  // ein paar Kilobyte und bauen sich beim nächsten Seitenaufruf von selbst
  // wieder auf - vor einem Versand sind sie besser weg. Das tat früher
  // genugSpeicher() und fiel mit der TLS-Speicherprüfung heraus; ohne den
  // Aufruf scheiterte der Versand am Gerät mit "send payload failed", weil
  // lwIP keine Sendepuffer mehr bekam.
  if (ESP.getFreeHeap() < AUFRAEUMEN_UNTER) {
    MemoryMgr.emergencyCleanup();
    yield();
  }

  char betreff[MailVorlage::BETREFF_MAX + 1];
  if (MailVorlagen::betreff(kind, betreff, sizeof(betreff)) == 0) {
    strncpy(betreff, "Pflanzensensor", sizeof(betreff) - 1);
    betreff[sizeof(betreff) - 1] = '\0';
  }

  // Rahmen, Stil und Vorlage in einen String rendern. Reserviert wird die
  // Serverobergrenze: ein einzelner Block statt einem Dutzend Umkopierungen
  // beim Wachsen, das schont eine ohnehin zerstückelte Halde.
  String rumpf;
  rumpf.reserve(MailClient::MAX_BODY_LEN);
  rumpf += F("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
             "<meta name=\"viewport\" content=\"width=device-width\"><style>");
  MailVorlagen::sendeStil(haengeAn, &rumpf);
  rumpf += F("</style></head><body>");
  MailVorlagen::sendeRumpf(kind, haengeAn, &rumpf);
  rumpf += F("</body></html>");

  // Lieber hier ablehnen als den Dienst mit "payload_too_large" antworten
  // lassen: die Meldung nennt die tatsächliche Länge, und der Nutzer kann sie
  // im Vorlageneditor direkt nachrechnen.
  if (rumpf.length() > MailClient::MAX_BODY_LEN) {
    fehler = String(F("Mailtext zu lang: ")) + rumpf.length() + F(" von ") +
             MailClient::MAX_BODY_LEN +
             F(" Bytes. Kuerze die Vorlage oder zeige weniger Sensoren.");
    return false;
  }

  // Der Punkt mit dem geringsten Vorrat: Rumpf im String, gleich kommen
  // Klartext-JSON, Chiffrat und Umschlag dazu. Zum Vergleich: der alte
  // TLS-Weg verbrauchte hier 11,7 KB und kam bis auf 3,9 KB herunter.
  const uint32_t freiVorher = ESP.getFreeHeap();
  const MailClient::SendResult ergebnis = MailClient::send(String(betreff), rumpf);
  if (ConfigMgr.isDebugMail()) {
    LOG_DEBUG(F("Mail"), String(F("Rumpf ")) + rumpf.length() + F(" B, Heap vor dem Versand ") +
                             freiVorher + F(" B, danach ") + ESP.getFreeHeap() + F(" B"));
  }
  if (!ergebnis.success) {
    fehler = ergebnis.message;
    // Hat der Mailbot geantwortet, ist das ein Urteil - falscher Schlüssel,
    // Tageslimit, zu lange Nutzlast. Das wiederholt sich nur. Kam die Anfrage
    // dagegen gar nicht durch, lohnt ein zweiter Versuch.
    //
    // Die Grenze ist <= 0, nicht == 0: der HTTPClient meldet Transportfehler
    // als negative Codes (-1 Verbindung, -3 "send payload failed"). Mit == 0
    // lief die Wiederholung ins Leere - genau der Fall, der am Gerät auftrat.
    m_voruebergehend = (ergebnis.httpStatus <= 0);
    return false;
  }
  m_voruebergehend = false;
  return true;
}
