/**
 * @file mail_manager.cpp
 * @brief SMTP Email Manager Implementation
 */

#include "mail_manager.h"

#if USE_MAIL

#include <ESP8266WiFi.h>
#include "managers/manager_config.h"

extern Logger logger; // Global logger instance

// Static instance
MailManager* MailManager::s_instance = nullptr;

MailManager& MailManager::getInstance() {
  if (s_instance == nullptr) {
    s_instance = new MailManager();
  }
  return *s_instance;
}

TypedResult<ResourceError, void> MailManager::initialize() {
  logger.info(F("MailManager"), F("Initialisiere SMTP Mail Manager"));

  // Prüfe WiFi-Verbindung
  if (WiFi.status() != WL_CONNECTED) {
    logger.error(F("MailManager"), F("Keine WiFi-Verbindung für E-Mail verfügbar"));
    return TypedResult<ResourceError, void>::fail(ResourceError::WIFI_ERROR,
                               F("Keine WiFi-Verbindung"));
  }

  m_initialized = true;
  logger.info(F("MailManager"), F("SMTP Mail Manager erfolgreich initialisiert"));

  // Sende Test-Mail beim Start wenn aktiviert
  if (ConfigMgr.isSmtpSendTestMailOnBoot()) {
    logger.info(F("MailManager"), F("Sende Test-Mail beim Start"));
    auto result = sendTestMail();
    if (!result.isSuccess()) {
      logger.warning(F("MailManager"), F("Test-Mail konnte nicht gesendet werden: ") +
                     result.getMessage());
    }
  }

  return TypedResult<ResourceError, void>::success();
}

ResourceResult MailManager::sendTestMail() {
  String subject = F(SMTP_SUBJECT);
  String message = F("Hallo!\n\n");
  message += F("Dies ist eine Test-E-Mail vom Pflanzensensor.\n");
  message += F("Gerätename: ") + String(F(DEVICE_NAME)) + F("\n");
  message += F("IP-Adresse: ") + WiFi.localIP().toString() + F("\n");
  message += F("WiFi SSID: ") + WiFi.SSID() + F("\n");
  message += F("Freier Speicher: ") + String(ESP.getFreeHeap()) + F(" Bytes\n");
  message += F("Uptime: ") + String(millis() / 1000) + F(" Sekunden\n\n");
  message += F("Viele Grüße,\n");
  message += F("Ihr Pflanzensensor");

  return sendMail(subject, message);
}

ResourceResult MailManager::sendMail(const String& subject, const String& message) {
  if (!m_initialized) {
    logger.error(F("MailManager"), F("Manager nicht initialisiert"));
    return ResourceResult::fail(ResourceError::INVALID_STATE,
                               F("Manager nicht initialisiert"));
  }

  logger.info(F("MailManager"), F("Sende E-Mail: ") + subject);

  // Create EMailSender instance with email credentials from ConfigManager
  EMailSender emailSend(ConfigMgr.getSmtpUser().c_str(),
                        ConfigMgr.getSmtpPassword().c_str(),
                        ConfigMgr.getSmtpSenderEmail().c_str(),
                        ConfigMgr.getSmtpSenderName().c_str());

  if (!setupEmailSender(emailSend)) {
    return ResourceResult::fail(ResourceError::CONFIG_ERROR,
                               F("EMailSender-Konfiguration fehlgeschlagen"));
  }

  // Create email message
  EMailSender::EMailMessage email;
  email.subject = subject;
  email.message = message;

  // Send email to default recipient
  String recipient = ConfigMgr.getSmtpRecipient();
  EMailSender::Response resp = emailSend.send(recipient.c_str(), email);

  if (resp.status) {
    logger.info(F("MailManager"), F("E-Mail erfolgreich gesendet"));
    return ResourceResult::success();
  } else {
    String errorMsg = F("E-Mail senden fehlgeschlagen: ") + resp.desc;
    logger.error(F("MailManager"), errorMsg);
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, errorMsg);
  }
}

ResourceResult MailManager::sendSensorAlarm(const String& sensorName,
                                           float value, float threshold) {
  String subject = F("⚠️ Sensor-Alarm: ") + sensorName;

  String message = F("SENSOR-ALARM!\n\n");
  message += F("Sensor: ") + sensorName + F("\n");
  message += F("Aktueller Wert: ") + String(value, 2) + F("\n");
  message += F("Grenzwert: ") + String(threshold, 2) + F("\n");
  message += F("Gerät: ") + ConfigMgr.getDeviceName() + F("\n");
  message += F("Zeit: ") + String(millis() / 1000) + F(" Sekunden seit Start\n\n");
  message += F("Bitte prüfen Sie den Sensor!\n\n");
  message += F("Ihr Pflanzensensor");

  return sendMail(subject, message);
}

bool MailManager::setupEmailSender(EMailSender& emailSend) {
  // Set SMTP server and port from ConfigManager
  emailSend.setSMTPServer(ConfigMgr.getSmtpHost().c_str());
  emailSend.setSMTPPort(ConfigMgr.getSmtpPort());

  if (ConfigMgr.isSmtpDebug()) {
    logger.debug(F("MailManager"), F("EMailSender konfiguriert für ") +
                 ConfigMgr.getSmtpHost() + F(":") + String(ConfigMgr.getSmtpPort()));
  }

  return true;
}

#endif // USE_MAIL
