/**
 * @file mail_sender.h
 * @brief Verschickt Warn-, Start- und Lebenszeichenmails per SMTP über TLS
 * @details Der Versand läuft ausschließlich aus loop(). Ein TLS-Handshake
 *          dauert auf diesem Gerät gut eine Sekunde und der ganze Versand
 *          mehrere; aus einem Webhandler heraus stünde so lange alles still,
 *          und die Speicherspitze fiele mit dem Antwortpuffer des Webservers
 *          zusammen.
 *
 *          Gemessen am Gerät (siehe Commit-Nachricht): der geprüfte Weg
 *          braucht rund 10,7 KB Heap in der Spitze, bei etwa 15 KB frei im
 *          Betrieb. Deshalb wird vor dem Verbinden der Handler-Cache geleert
 *          und unterhalb einer Schwelle gar nicht erst begonnen.
 */

#ifndef MAIL_SENDER_H
#define MAIL_SENDER_H

#include <Arduino.h>

#include "utils/mail_scheduler.h"

class MailSender {
public:
  static constexpr uint32_t TEST_GEDULD_MS = 120000;
  /// Takt, in dem eine wartende Testmail es erneut versucht. Nicht kürzer:
  /// jeder Versuch räumt den Handler-Cache des Webservers leer, und der muss
  /// sich danach neu aufbauen - im Sekundentakt macht das die Oberfläche zäh.
  static constexpr uint32_t TEST_ABSTAND_MS = 5000;
  static constexpr uint32_t MIN_FREE_BLOCK = 9000;

  static MailSender& instance();

  /// Konfiguration übernehmen und die Sperren aus der letzten Laufzeit laden.
  void begin();
  /// Aus loop() aufrufen. Sendet höchstens eine Mail je Aufruf.
  void loop();
  /// Konfiguration hat sich geändert (Weboberfläche)
  void reloadConfig();

  /// Testmail anfordern. Gesendet wird erst im nächsten loop() - siehe oben.
  void requestTestMail();
  bool testMailPending() const { return m_testPending; }
  /// Ergebnis der letzten Sendung, für die Anzeige in der Weboberfläche
  const String& lastResult() const { return m_lastResult; }
  uint32_t lastAttempt() const { return m_lastAttempt; }
  uint32_t sentCount() const { return m_sent; }

private:
  MailSender() = default;

  void aktualisiereZustaende();
  bool sende(Mail::Kind kind, String& fehler);

  /// Server, bei dem die Prüfung schon einmal scheiterte. Ein zweiter Anlauf
  /// kostet nur Zeit und Speicher - die eine mitgelieferte Wurzel passt dann
  /// eben nicht.

  Mail::Scheduler m_scheduler;
  bool m_testPending{false};
  uint32_t m_lastAttempt{0};
  uint32_t m_sent{0};
  uint32_t m_lastCheck{0};
  String m_lastResult;
  /// Bis wann eine angeforderte Testmail bei Speichermangel erneut versucht
  /// wird. Ein Fehlschlag um ein paar hundert Byte ist ein Momentzustand -
  /// gleich darauf gibt der Webserver seine Puffer frei.
  uint32_t m_testFristMs{0};
  /// War der letzte Fehlschlag vorübergehend (kein WLAN, keine Uhrzeit)? Dann
  /// lohnt ein neuer Versuch; eine Absage des Mailbots wiederholt sich nur.
  bool m_voruebergehend{false};
};

#endif // MAIL_SENDER_H
