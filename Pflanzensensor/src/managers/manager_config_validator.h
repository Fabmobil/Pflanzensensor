/**
 * @file manager_config_validator.h
 * @brief Configuration validation logic
 */

#ifndef MANAGER_CONFIG_VALIDATOR_H
#define MANAGER_CONFIG_VALIDATOR_H

#include "../utils/result_types.h"
#include "configs/config_validation_rules.h"
#include "manager_config_types.h"

class ConfigValidator {
public:
  using ValidationResult = TypedResult<ConfigError, void>;

  /**
   * @brief Validates a password
   * @param password The password to validate
   * @return ValidationResult indicating success or failure
   */
  static ValidationResult validatePassword(const String& password);

  /**
   * @brief Validates a log level string
   * @param level The log level string to validate
   * @return ValidationResult indicating success or failure
   */
  static ValidationResult validateLogLevel(const String& level);

  /**
   * @brief Validate entire configuration data structure
   * @param config Configuration data to validate
   * @return ValidationResult indicating success or failure
   */
  static ValidationResult validateConfigData(const ConfigData& config);

private:
  ConfigValidator() = default;
};

#endif
