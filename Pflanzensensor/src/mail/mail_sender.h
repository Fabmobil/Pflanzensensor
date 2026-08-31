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
  /// Untergrenzen für einen Sendeversuch, gemessen am Gerät.
  ///
  /// Bindend ist der größte zusammenhängende Block, nicht der freie Heap
  /// insgesamt: BearSSL fordert viele kleine bis mittlere Stücke an, das größte
  /// davon liegt bei gut 4 KB. Eine Blockschwelle von 17 KB, wie hier zuerst
  /// eingetragen, verhinderte den Versand bei 30 % Fragmentierung grundlos.
  ///
  /// Der freie Heap dagegen ist bindend: gemessen am Gerät verbraucht der
  /// Handshake 11720 Byte (18912 vor, 7192 danach). Darunter darf nicht
  /// gestartet werden - der Speicher geht mitten im Handshake aus, und das
  /// endet in "Unhandled C++ exception: OOM" samt Neustart im Betrieb.
  /// 14000 lässt gut 2 KB für alles, was während des Versands weiterläuft.
  ///
  /// Vorher standen hier 15000, eine Zahl aus der Zeit der
  /// Zertifikatsprüfung - die brauchte gut 5 KB mehr. Ohne sie war das zu
  /// streng: nach ein paar Seitenaufrufen steht das Gerät bei knapp 15 KB, und
  /// eine Testmail scheiterte an 64 fehlenden Bytes.
  static constexpr uint32_t MIN_FREE_HEAP = 14000;
  /// So lange wird eine angeforderte Testmail bei Speichermangel wiederholt.
  static constexpr uint32_t TEST_GEDULD_MS = 120000;
  /// Takt, in dem eine wartende Testmail es erneut versucht.
  static constexpr uint32_t TEST_ABSTAND_MS = 2000;
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
  bool genugSpeicher(String& fehler);

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
  /// War der letzte Fehlschlag bloß Speichermangel? Dann lohnt ein neuer
  /// Versuch; eine Absage des Servers dagegen wiederholt sich nur.
  bool m_speichermangel{false};
};

#endif // MAIL_SENDER_H
