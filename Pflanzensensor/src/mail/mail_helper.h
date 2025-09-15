/**
 * @file mail_helper.h
 * @brief Helper-Funktionen für E-Mail-Versand
 */

#ifndef MAIL_HELPER_H
#define MAIL_HELPER_H

#include "../configs/config.h"

#if USE_MAIL

#include "../utils/result_types.h"
#include <Arduino.h>

/**
 * @namespace MailHelper
 * @brief Helper-Funktionen für E-Mail-Operationen
 */
namespace MailHelper {
  /**
   * @brief Sende schnell eine Test-E-Mail
   * @return ResourceResult mit Erfolgsstatus
   */
  ResourceResult sendQuickTestMail();

  /**
   * @brief Sende E-Mail mit System-Info
   * @return ResourceResult mit Erfolgsstatus
   */
  ResourceResult sendSystemInfo();

  /**
   * @brief Prüfe ob E-Mail-System bereit ist
   * @return true wenn bereit für E-Mail-Versand
   */
  bool isMailSystemReady();

  /**
   * @brief Formatiere Systemstatistiken als String
   * @return String mit Systeminfo
   */
  String getSystemInfoString();
}

#endif // USE_MAIL
#endif // MAIL_HELPER_H
