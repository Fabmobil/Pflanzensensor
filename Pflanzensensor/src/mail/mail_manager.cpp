#include "mail_manager.h"

#if USE_MAIL

#include "../configs/config.h"
#include "../configs/config_pflanzensensor.h"
#include "../managers/manager_config.h"
#include "../managers/manager_resource.h"
#include <ReadyMail.h>
#include <WiFiClientSecure.h>

using namespace ReadyMailSMTP;
extern Logger logger;
extern ResourceManager& ResourceMgr;

// Static member definition
MailManager* MailManager::s_instance = nullptr;

MailManager& MailManager::getInstance() {
  if (!s_instance) {
    s_instance = new MailManager();
  }
  return *s_instance;
}

TypedResult<ResourceError, void> MailManager::initialize() {
  if (m_initialized) {
    return ResourceResult::success();
  }

  setState(ManagerState::INITIALIZING);

  logger.info(F("MailManager"), F("Initialisiere ReadyMail SMTP Manager"));

  if (!ConfigMgr.isMailEnabled()) {
    logger.info(F("MailManager"), F("Mail-Funktionalität ist deaktiviert"));
    m_initialized = true;
    setState(ManagerState::INITIALIZED);
    return ResourceResult::success();
  }

  logger.info(F("MailManager"), F("ReadyMail SMTP Manager erfolgreich initialisiert"));
  m_initialized = true;
  setState(ManagerState::INITIALIZED);

  // Send test email if configured
  if (ConfigMgr.isSmtpSendTestMailOnBoot()) {
    // Check available memory before test
    uint32_t freeHeap = ESP.getFreeHeap();
    logger.debug(F("MailManager"),
                 F("Freier Speicher für Test-Mail: ") + String(freeHeap) + F(" Bytes"));

    if (freeHeap >= SMTP_MIN_FREE_HEAP_FOR_TEST) { // configurable minimum free heap
      logger.info(F("MailManager"), F("Sende Test-Mail beim Start"));
      ResourceResult testResult = sendTestMail();
      if (testResult.isError()) {
        logger.warning(F("MailManager"), F("Test-Mail fehlgeschlagen"));
      }
    } else {
      logger.warning(F("MailManager"), F("Nicht genug Speicher für Test-Mail"));
    }
  }

  return ResourceResult::success();
}

ResourceResult MailManager::sendTestMail() {
  return sendMail(
      F("Test Mail"),
      F("Test-Mail vom Pflanzensensor.<br/><br/>"
        "Wenn Sie diese Nachricht erhalten, funktioniert die E-Mail-Konfiguration korrekt."));
}

ResourceResult MailManager::sendMail(const String& subject, const String& message) {
  if (!m_initialized) {
    logger.error(F("MailManager"), F("MailManager nicht initialisiert"));
    return ResourceResult::fail(ResourceError::INVALID_STATE, F("MailManager nicht initialisiert"));
  }

  if (!ConfigMgr.isMailEnabled()) {
    logger.error(F("MailManager"), F("Mail-Funktionalität ist deaktiviert"));
    return ResourceResult::fail(ResourceError::INVALID_STATE,
                                F("Mail-Funktionalität ist deaktiviert"));
  }

  // Check available memory before attempting to send email
  uint32_t freeHeapBefore = ESP.getFreeHeap();
  logger.debug(F("MailManager"),
               F("Freier Speicher vor E-Mail: ") + String(freeHeapBefore) + F(" Bytes"));

  // Require at least 12KB free heap for SSL operations (increased from 8KB)
  if (freeHeapBefore < SMTP_MIN_FREE_HEAP_FOR_TEST) {
    logger.error(F("MailManager"), F("Nicht genug Speicher für E-Mail"));
    return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY, F("Nicht genügend Speicher"));
  }

  logger.info(F("MailManager"), F("Sende E-Mail"));

  // Get configuration
  String host = ConfigMgr.getSmtpHost();
  uint16_t port = ConfigMgr.getSmtpPort();
  String username = ConfigMgr.getSmtpUser();
  String password = ConfigMgr.getSmtpPassword();
  String senderName = ConfigMgr.getSmtpSenderName();
  String senderEmail = ConfigMgr.getSmtpSenderEmail();
  String recipient = ConfigMgr.getSmtpRecipient();
  bool useStartTLS = ConfigMgr.isSmtpEnableStartTLS();

  // Yield to system before memory-intensive operations
  yield();

  // Skip basic connectivity testing - ESP8266 plain connect() often fails on encrypted ports
  // Determine connection type based on port and STARTTLS setting
  bool useDirectSSL = (port == 465);
  bool usePlainConnection = (port == 25);
  bool useSTARTTLS = (port == 587 && useStartTLS);

  logger.debug(F("MailManager"), F("ESP8266 SMTP-Verbindung"));

  // For ESP8266 compatibility: Try multiple approaches for port 587
  if (useSTARTTLS && port == 587) {
    logger.debug(F("MailManager"), F("Verwende STARTTLS"));

    // First attempt: WiFiClientSecure with insecure mode for STARTTLS
    BearSSL::WiFiClientSecure ssl_client;
    ssl_client.setInsecure();            // Skip certificate validation
    ssl_client.setBufferSizes(512, 512); // Smaller buffers for ESP8266
    ReadyMailSMTP::SMTPClient smtp_ssl(ssl_client);

    ResourceResult result =
        performSMTPOperations(smtp_ssl, host, port, username, password, senderName, senderEmail,
                              recipient, subject, message, true);

    if (result.isError()) {
      logger.warning(F("MailManager"), F("Port 587 fehlgeschlagen, teste Port 465"));

      // Fallback: Try port 465 direct SSL
      port = 465;
      useDirectSSL = true;
      useSTARTTLS = false;

      BearSSL::WiFiClientSecure ssl_client_465;
      ssl_client_465.setInsecure();
      ssl_client_465.setBufferSizes(512, 512);
      ReadyMailSMTP::SMTPClient smtp_465(ssl_client_465);

      logger.debug(F("MailManager"), F("Versuche Port 465"));
      return performSMTPOperations(smtp_465, host, port, username, password, senderName,
                                   senderEmail, recipient, subject, message, true);
    }

    return result;
  }

  // Handle other connection types
  if (usePlainConnection) {
    // Use plain WiFiClient for port 25 (no encryption)
    WiFiClient client;
    ReadyMailSMTP::SMTPClient smtp(client);
    logger.debug(F("MailManager"), F("Verwende plain Client"));

    return performSMTPOperations(smtp, host, port, username, password, senderName, senderEmail,
                                 recipient, subject, message, false);
  } else if (useDirectSSL) {
    // Use direct SSL client for port 465 (smtps)
    BearSSL::WiFiClientSecure ssl_client;
    ssl_client.setInsecure();            // Skip certificate validation for ESP8266
    ssl_client.setBufferSizes(512, 512); // Smaller buffers for stability
    ReadyMailSMTP::SMTPClient smtp(ssl_client);
    logger.debug(F("MailManager"), F("Verwende Direct SSL"));

    return performSMTPOperations(smtp, host, port, username, password, senderName, senderEmail,
                                 recipient, subject, message, true);
  } else {
    // Fallback for unexpected configuration
    logger.error(F("MailManager"), F("Unbekannte SMTP-Konfiguration"));
    return ResourceResult::fail(ResourceError::CONFIG_ERROR, F("Unbekannte SMTP-Konfiguration"));
  }
}

ResourceResult MailManager::performSMTPOperations(
    ReadyMailSMTP::SMTPClient& smtp, const String& host, uint16_t port, const String& username,
    const String& password, const String& senderName, const String& senderEmail,
    const String& recipient, const String& subject, const String& message, bool useDirectSSL) {

  // Status callback for debugging - minimal logging to save memory
  auto statusCallback = [](ReadyMailSMTP::SMTPStatus status) {
    extern Logger logger;
    if (status.isComplete) {
      if (status.errorCode < 0) {
        logger.error(F("MailManager"), F("SMTP Error"));
      } else {
        logger.debug(F("MailManager"), F("SMTP OK"));
      }
    }
    // Skip non-complete status logging to save memory
  };

  logger.debug(F("MailManager"), F("Verbinde zu SMTP Server"));
  yield(); // Give system time before heavy operations

  // For port 587, enable STARTTLS; for port 465, use direct SSL
  bool enableSTARTTLS = (port == 587);

  // Connect to server
  if (!smtp.connect(host, port, statusCallback, useDirectSSL, enableSTARTTLS)) {
    logger.error(F("MailManager"), F("SMTP Verbindung fehlgeschlagen"));
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("SMTP Verbindung fehlgeschlagen"));
  }

  if (!smtp.isConnected()) {
    logger.error(F("MailManager"), F("SMTP Server nicht verbunden"));
    return ResourceResult::fail(ResourceError::WIFI_ERROR, F("SMTP Server nicht verbunden"));
  }

  logger.debug(F("MailManager"), F("SMTP Verbindung erfolgreich"));

  // Authenticate using username/password
  if (!smtp.authenticate(username, password, readymail_auth_password, true)) {
    logger.error(F("MailManager"), F("SMTP Authentifizierung fehlgeschlagen"));
    smtp.stop();
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                F("SMTP Authentifizierung fehlgeschlagen"));
  }

  if (!smtp.isAuthenticated()) {
    logger.error(F("MailManager"), F("SMTP nicht authentifiziert"));
    smtp.stop();
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("SMTP nicht authentifiziert"));
  }

  logger.debug(F("MailManager"), F("SMTP Authentifizierung erfolgreich"));

  // Create message following ReadyMail v0.3.0+ pattern
  ReadyMailSMTP::SMTPMessage& msg = smtp.getMessage();

  // Set headers using the new API pattern
  msg.headers.add(ReadyMailSMTP::rfc822_from, senderName + " <" + senderEmail + ">");
  msg.headers.add(ReadyMailSMTP::rfc822_subject, subject);
  msg.headers.add(ReadyMailSMTP::rfc822_to, recipient);

  // Set message body (both text and HTML)
  msg.text.body(message);
  msg.html.body(message);

  // Set timestamp for Date header (important for spam prevention)
  msg.timestamp = 1700000000; // Some time in 2023 - better than no timestamp

  yield();

  // Send message using internal message (required for v0.3.0+)
  if (!smtp.send(msg, "", true)) {
    logger.error(F("MailManager"), F("E-Mail senden fehlgeschlagen"));
    smtp.stop();
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("E-Mail senden fehlgeschlagen"));
  }

  // Check final status
  ReadyMailSMTP::SMTPStatus finalStatus = smtp.status();
  if (finalStatus.errorCode < 0) {
    logger.error(F("MailManager"), F("E-Mail Fehler: ") + String(finalStatus.errorCode));
    smtp.stop();
    return ResourceResult::fail(ResourceError::OPERATION_FAILED,
                                F("E-Mail Fehler: ") + String(finalStatus.errorCode));
  }

  smtp.stop();

  uint32_t freeHeapAfter = ESP.getFreeHeap();
  logger.info(F("MailManager"), F("E-Mail erfolgreich gesendet"));
  logger.debug(F("MailManager"),
               F("Freier Speicher nach E-Mail: ") + String(freeHeapAfter) + F(" Bytes"));

  yield();
  return ResourceResult::success();
}

#endif // USE_MAIL
