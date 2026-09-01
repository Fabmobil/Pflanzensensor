/**
 * @file mail_client.h
 * @brief HTTP-Transport für Alarm-Mails zum Mailbot-Webservice
 * @details Baut die Klartext-JSON-Nutzlast, verschlüsselt sie über
 *          MailCrypto::seal() und schickt den JSON-Umschlag per
 *          Klartext-HTTP-POST an den in der Konfiguration hinterlegten
 *          Mailbot. Nutzt ESP8266HTTPClient (Teil des ESP8266-Arduino-Cores,
 *          kein zusätzlicher lib_dep).
 */

#ifndef MAIL_CLIENT_H
#define MAIL_CLIENT_H

#include <Arduino.h>

class MailClient {
public:
  /// Grenzen des Dienstes (max_subject_length / max_body_length in dessen
  /// config.php). Hier gespiegelt, damit die Firmware einen zu langen Text
  /// erkennbar ablehnt, statt ihn zu verschlüsseln, zu verschicken und dann
  /// ein "payload_too_large" zurückzubekommen.
  static constexpr size_t MAX_SUBJECT_LEN = 120;
  static constexpr size_t MAX_BODY_LEN = 2000;

  struct SendResult {
    bool success = false;
    int httpStatus = 0;
    String message; // Klartext-Grund bei Misserfolg, für die Test-Mail-Anzeige
  };

  /**
   * @brief Verschlüsselt und verschickt eine Alarm-/Test-Mail
   * @param subject Betreff (Klartext, wird verschlüsselt übertragen)
   * @param body Inhalt (Klartext, wird verschlüsselt übertragen)
   * @details Liest Empfänger, Service-URL, Geräte-ID und Geheimschlüssel
   *          selbst aus ConfigMgr. Sendet nicht, wenn Mail deaktiviert ist,
   *          WiFi nicht verbunden ist, keine gültige NTP-Zeit vorliegt, oder
   *          eines der Pflichtfelder leer ist - jeweils mit erklärender
   *          Meldung im Rückgabewert.
   */
  static SendResult send(const String& subject, const String& body);
};

#endif // MAIL_CLIENT_H
