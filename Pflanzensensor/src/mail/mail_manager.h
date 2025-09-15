/**
 * @file mail_manager.h
 * @brief SMTP Email Manager für ESP8266 Pflanzensensor
 * @details Manager für das Senden von E-Mails über SMTP
 */

#ifndef MAIL_MANAGER_H
#define MAIL_MANAGER_H

#include "../configs/config.h"

#if USE_MAIL

#include "../logger/logger.h"
#include "../managers/manager_base.h"
#include "../utils/result_types.h"
#include <EMailSender.h>

/**
 * @class MailManager
 * @brief Manager für SMTP E-Mail-Funktionalität
 * @details Singleton-Manager der das Senden von E-Mails über SMTP ermöglicht
 */
class MailManager : public Manager {
 public:
  /**
   * @brief Get singleton instance
   * @return Reference to MailManager instance
   */
  static MailManager& getInstance();

  /**
   * @brief Sende eine Test-E-Mail
   * @return ResourceResult mit Erfolgsstatus
   */
  ResourceResult sendTestMail();

  /**
   * @brief Sende E-Mail mit benutzerdefinierten Inhalt
   * @param subject Betreff der E-Mail
   * @param message Nachricht der E-Mail
   * @return ResourceResult mit Erfolgsstatus
   */
  ResourceResult sendMail(const String& subject, const String& message);

  /**
   * @brief Sende Sensor-Alarm E-Mail
   * @param sensorName Name des Sensors
   * @param value Gemessener Wert
   * @param threshold Grenzwert
   * @return ResourceResult mit Erfolgsstatus
   */
  ResourceResult sendSensorAlarm(const String& sensorName, float value,
                                float threshold);

 protected:
  /**
   * @brief Initialize the mail manager
   * @return ResourceResult indicating success or failure
   */
  TypedResult<ResourceError, void> initialize() override;

 private:
  /**
   * @brief Konstruktor (Singleton)
   */
  MailManager() : Manager("MailManager") {}

  /**
   * @brief Setup EMailSender configuration
   * @param emailSend EMailSender object reference
   * @return true wenn Setup erfolgreich
   */
  bool setupEmailSender(EMailSender& emailSend);

  static MailManager* s_instance; ///< Singleton instance
  bool m_initialized = false;     ///< Initialisierungsstatus
};

#endif // USE_MAIL
#endif // MAIL_MANAGER_H
