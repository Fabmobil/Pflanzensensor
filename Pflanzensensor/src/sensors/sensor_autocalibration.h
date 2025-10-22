#ifndef SENSOR_AUTOCALIBRATION_H
#define SENSOR_AUTOCALIBRATION_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @file sensor_autocalibration.h
 * @brief Exponential Moving Boundaries (Auto-Calibration) helper
 *
 * Provides a small POD struct and helper functions to maintain an
 * exponentially-adapting observed min/max range for analog sensors.
 * The implementation is intentionally lightweight and avoids dynamic
 * allocations.
 */

typedef struct AutoCal {
  // Integer values used for persistence/JSON compatibility
  uint16_t min_value{0};
  uint16_t max_value{1023};
  // Internal float-backed EMA state to allow very small gradual changes
  // without being immediately rounded away. These are not written to JSON
  // (we persist integer fields), but are used during updates to accumulate
  // fractional adjustments.
  float min_value_f{0.0f};
  float max_value_f{1023.0f};
  uint32_t last_update_time{0};  // minutes since start
} AutoCal;

/**
 * Initialize an AutoCal block with an initial reading and timestamp.
 * Sets min/max := initial_reading and last_update_time := current_time
 */
void AutoCal_init(AutoCal& cal, float initial_reading,
                  uint32_t current_time_minutes);

/**
 * Update AutoCal with a new raw reading. Uses an Exponential Moving
 * Boundaries rule with the provided alpha. Returns true when either
 * min_value or max_value changed.
 *
 * Notes:
 * - alpha should be small (e.g. 0.0001f) for very slow adaptation.
 * - last_update_time will be set to current_time_minutes on every call.
 */
bool AutoCal_update(AutoCal& cal, uint16_t new_reading,
                    uint32_t current_time_minutes,
                    float alpha = 0.0001f);

/**
 * Deserialize AutoCal from a JsonObject if fields exist. Missing fields
 * are left unchanged. Returns true if any field was updated.
 */
bool AutoCal_from_json(const JsonObject& obj, AutoCal& cal);

/**
 * Serialize AutoCal into a JsonObject. Overwrites/creates fields
 * "min_value", "max_value" and "last_update_time".
 */
void AutoCal_to_json(const AutoCal& cal, JsonObject& obj);

#endif  // SENSOR_AUTOCALIBRATION_H
