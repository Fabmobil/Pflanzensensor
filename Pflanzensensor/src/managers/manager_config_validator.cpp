/**
 * @file manager_config_validator.cpp
 * @brief Implementation of ConfigValidator class
 */

#include "manager_config_validator.h"

#include "../logger/logger.h"

ConfigValidator::ValidationResult ConfigValidator::validatePassword(const String& password) {
  if (password.length() < ConfigValidationRules::MIN_PASSWORD_LENGTH ||
      password.length() > ConfigValidationRules::MAX_PASSWORD_LENGTH) {
    return ValidationResult::fail(
        ConfigError::VALIDATION_ERROR,
        F("Passwortlänge muss zwischen ") + String(ConfigValidationRules::MIN_PASSWORD_LENGTH) +
            F(" und ") + String(ConfigValidationRules::MAX_PASSWORD_LENGTH) + F(" Zeichen liegen"));
  }
  // Enforce ASCII-only characters (no Unicode). Accept bytes in range 0x20-0x7E
  for (size_t i = 0; i < password.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(password[i]);
    if (c < 0x20 || c > 0x7E) {
      return ValidationResult::fail(
          ConfigError::VALIDATION_ERROR,
          F("Nur ASCII-Zeichen erlaubt (keine Sonderzeichen außerhalb von 0x20-0x7E)"));
    }
  }
  return ValidationResult::success();
}

ConfigValidator::ValidationResult ConfigValidator::validateLogLevel(const String& level) {
  // Check if the log level is valid by attempting to convert it
  if (level != "DEBUG" && level != "INFO" && level != "WARNING" && level != "ERROR") {
    return ValidationResult::fail(ConfigError::VALIDATION_ERROR,
                                  F("Ungültiges Log-Level: ") + level);
  }
  return ValidationResult::success();
}

ConfigValidator::ValidationResult ConfigValidator::validateConfigData(const ConfigData& config) {
  // Validate password
  auto passwordResult = validatePassword(config.adminPassword);
  if (!passwordResult.isSuccess()) {
    return passwordResult;
  }

  return ValidationResult::success();
}
