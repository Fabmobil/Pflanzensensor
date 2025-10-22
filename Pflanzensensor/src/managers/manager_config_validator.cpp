/**
 * @file manager_config_validator.cpp
 * @brief Implementation of ConfigValidator class
 */

#include "manager_config_validator.h"

#include "../logger/logger.h"

ConfigValidator::ValidationResult ConfigValidator::validatePassword(const String& password) {
  if (password.length() < ConfigValidationRules::MIN_PASSWORD_LENGTH ||
      password.length() > ConfigValidationRules::MAX_PASSWORD_LENGTH) {
    return ValidationResult::fail(ConfigError::VALIDATION_ERROR,
                                  F("Passwortlänge muss zwischen 8 und 32 Zeichen liegen"));
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
