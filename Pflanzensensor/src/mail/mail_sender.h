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
  /// Untergrenzen für einen Sendeversuch. Die Zahlen stammen aus einer Messung
  /// am Gerät: ein TLS-Handshake mit Zertifikatsprüfung verbrauchte 10,7 KB
  /// Heap in der Spitze und kam bis auf 6,4 KB frei herunter - und das mit der
  /// kurzen Kette von Let's Encrypt. Mit 13 KB Vorrat endete derselbe Versuch
  /// gegen einen Server mit längerer Kette in "Unhandled C++ exception: OOM"
  /// und einem Neustart mitten im Betrieb. Deshalb hier reichlich Abstand.
  /// Bindend ist der freie Heap insgesamt, nicht der größte Block: BearSSL
  /// fordert viele kleine bis mittlere Stücke an, das größte davon liegt bei
  /// gut 4 KB. Im Spike genügte ein größter Block von 9992 B für den geprüften
  /// Handshake - eine Blockschwelle von 17 KB, wie hier zuerst eingetragen,
  /// verhinderte den Versand bei 30 % Fragmentierung völlig grundlos.
  /// 21 KB, nicht 19: ein gescheiterter Prüfversuch verbraucht gemessene 7,1 KB
  /// und hinterlässt eine zerstückelte Halde. Wird er bei 19,6 KB begonnen,
  /// bleiben danach 12,5 KB - zu wenig für den ungeprüften zweiten Anlauf, und
  /// der ganze Versand scheitert. Lieber gar nicht erst prüfen als deshalb
  /// keine Mail verschicken.
  static constexpr uint32_t MIN_FREE_HEAP_VERIFIED = 21000;
  static constexpr uint32_t MIN_FREE_BLOCK_VERIFIED = 9500;
  /// Ohne Prüfung entfallen Vertrauensanker und Kettenprüfung.
  static constexpr uint32_t MIN_FREE_HEAP = 15000;
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
  /// Reicht der Speicher zusätzlich für die Zertifikatsprüfung?
  bool genugSpeicherFuerPruefung() const;

  /// Server, bei dem die Prüfung schon einmal scheiterte. Ein zweiter Anlauf
  /// kostet nur Zeit und Speicher - die eine mitgelieferte Wurzel passt dann
  /// eben nicht.
  String m_ungepruefterHost;

  Mail::Scheduler m_scheduler;
  bool m_testPending{false};
  uint32_t m_lastAttempt{0};
  uint32_t m_sent{0};
  uint32_t m_lastCheck{0};
  String m_lastResult;
};

#endif // MAIL_SENDER_H
