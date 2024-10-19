/**
 * @file webhook.h
 * @brief Webhook-Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Kommunikation mit einem Webhook-Dienst,
 * um Benachrichtigungen und Daten an externe Systeme zu senden.
 */

#ifndef WEBHOOK_H
#define WEBHOOK_H

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

extern bool vorherAlarm;
extern String letzterWebhookStatus;
extern String webhookStatus;
extern const int httpsPort;
extern String webhookDomain;
extern String webhookPfad;
extern int webhookPingFrequenz;
extern int webhookFrequenz;
extern int neustarts;

/**
 * @brief Initialisiert das Webhook-Modul
 *
 * Diese Funktion synchronisiert die Zeit, initialisiert die Zertifikate
 * und sendet eine Initialisierungsnachricht an den Webhook-Dienst.
 */
void WebhookSetup();

/**
 * @brief Sendet eine Initialisierungsnachricht über den Webhook
 *
 * Diese Funktion erstellt eine JSON-Nachricht mit Initialisierungsdaten
 * und sendet sie über den konfigurierten Webhook.
 */
void WebhookSendeInit();

/**
 * @brief Erfasst die aktuellen Sensordaten und sendet sie über den Webhook
 *
 * @param statusWert Der aktuelle Status des Sensors ("ping", "normal", etc.)
 */
void WebhookErfasseSensordaten(const char* statusWert);

/**
 * @brief Sendet die gesammelten Daten über den Webhook
 *
 * @param jsonString Die zu sendenden Daten als JSON-String
 */
void WebhookSendeDaten(const String& jsonString);

/**
 * @brief Aktualisiert den Alarmstatus basierend auf den aktuellen Sensorwerten
 *
 * @return bool true wenn ein Alarm vorliegt, sonst false
 */
bool WebhookAktualisiereAlarmStatus();

#endif // WEBHOOK_H
