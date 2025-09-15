/**
 * @file config_validation_rules.h
 * @brief Defines validation rules for configuration parameters
 */

#pragma once

namespace ConfigValidationRules {
const unsigned long MIN_INTERVAL = 1 * 1000;                 // 1 second
const unsigned long MAX_INTERVAL = 60 * 60 * 1000;           // 1 hour
const unsigned long MIN_SDS011_INTERVAL = 30 * 1000;         // 30 seconds
const unsigned long MAX_SDS011_INTERVAL = 10 * 3600 * 1000;  // 10 hour
const size_t MIN_PASSWORD_LENGTH = 8;
const size_t MAX_PASSWORD_LENGTH = 32;
}  // namespace ConfigValidationRules
