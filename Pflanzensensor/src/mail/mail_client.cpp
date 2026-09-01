/**
 * @file mail_client.cpp
 * @brief Implementierung siehe mail_client.h
 */

#include "mail_client.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "mail_crypto.h"

#include "../logger/logger.h"
#include "../managers/manager_config.h"
#include "../utils/base64.h"

namespace {
// ArduinoJson 7 verwaltet den Speicher selbst und wächst nach Bedarf;
// StaticJsonDocument<N> gibt es dort nicht mehr. Die Obergrenze für den Rumpf
// setzt deshalb der Aufrufer (MailSender prüft gegen
// MailClient::MAX_BODY_LEN), nicht mehr eine feste Dokumentgröße.
} // namespace

MailClient::SendResult MailClient::send(const String& subject, const String& body) {
  SendResult result;

  if (!ConfigMgr.isMailEnabled()) {
    result.message = F("Mail-Benachrichtigungen sind deaktiviert");
    return result;
  }
  if (WiFi.status() != WL_CONNECTED) {
    result.message = F("Keine WLAN-Verbindung");
    return result;
  }
  if (!logger.isNTPInitialized()) {
    result.message = F("Keine gültige NTP-Zeit verfügbar");
    return result;
  }

  String url = ConfigMgr.getMailServiceUrl();
  String deviceId = ConfigMgr.getMailDeviceId();
  String secretKeyBase64 = ConfigMgr.getMailSecretKey();
  String recipient = ConfigMgr.getMailTo();
  if (url.isEmpty() || deviceId.isEmpty() || secretKeyBase64.isEmpty() || recipient.isEmpty()) {
    result.message = F("Mail-Konfiguration unvollständig (URL/Geräte-ID/Schlüssel/Empfänger)");
    return result;
  }

  if (subject.length() > MAX_SUBJECT_LEN || body.length() > MAX_BODY_LEN) {
    result.message = String(F("Zu lang für den Mailbot: Betreff ")) + subject.length() + F("/") +
                     MAX_SUBJECT_LEN + F(" B, Inhalt ") + body.length() + F("/") + MAX_BODY_LEN +
                     F(" B");
    return result;
  }

  uint8_t secretKey[MailCrypto::KEY_LEN];
  if (Base64::decode(secretKeyBase64, secretKey, sizeof(secretKey)) !=
      static_cast<int>(MailCrypto::KEY_LEN)) {
    result.message = F("Geräteschlüssel ungültig (nicht 32 Byte Base64)");
    return result;
  }

  JsonDocument plainDoc;
  plainDoc["to"] = recipient;
  plainDoc["subject"] = subject;
  plainDoc["body"] = body;
  String plaintext;
  serializeJson(plainDoc, plaintext);

  uint32_t ts = static_cast<uint32_t>(logger.getSynchronizedTime());
  MailCrypto::SealedEnvelope sealed = MailCrypto::seal(secretKey, plaintext, deviceId, ts);
  if (!sealed.success) {
    result.message = F("Verschlüsselung fehlgeschlagen (Zufallszahlengenerator)");
    return result;
  }

  JsonDocument envelopeDoc;
  envelopeDoc["device_id"] = deviceId;
  envelopeDoc["ts"] = ts;
  envelopeDoc["nonce"] = sealed.nonceBase64;
  envelopeDoc["ciphertext"] = sealed.ciphertextBase64;
  envelopeDoc["tag"] = sealed.tagBase64;
  String payload;
  serializeJson(envelopeDoc, payload);

  WiFiClient wifiClient;
  HTTPClient http;
  // "Mailbot-URL" zeigt direkt auf den Endpunkt-Ordner (z.B.
  // "http://host:port", der Port ist ein rohes TCP-Port-Forwarding am
  // Uberspace-Account, dessen Docroot bereits public/api/ ist - siehe
  // Deployment-Notizen im Mailbot-Repo, README.md). Kein "/api"-Präfix.
  String endpoint = url + F("/send.php");
  if (!http.begin(wifiClient, endpoint)) {
    result.message = F("Verbindungsaufbau zum Mailbot fehlgeschlagen");
    return result;
  }
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-Device-Mac"), WiFi.macAddress());

  int httpCode = http.POST(payload);
  result.httpStatus = httpCode;

  if (httpCode == HTTP_CODE_OK) {
    result.success = true;
    result.message = F("Mail erfolgreich verschickt");
  } else if (httpCode > 0) {
    String responseBody = http.getString();
    JsonDocument errorDoc;
    if (deserializeJson(errorDoc, responseBody) == DeserializationError::Ok &&
        errorDoc["error"].is<const char*>()) {
      result.message =
          String(F("Mailbot lehnte ab (")) + httpCode + F("): ") + errorDoc["error"].as<String>();
    } else {
      result.message = String(F("Mailbot antwortete mit Status ")) + httpCode;
    }
  } else {
    result.message = String(F("HTTP-Anfrage fehlgeschlagen: ")) + http.errorToString(httpCode);
  }

  http.end();
  return result;
}
