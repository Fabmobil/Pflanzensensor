#include "sensors/sensor_autocalibration.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>

void AutoCal_init(AutoCal& cal, float initial_reading,
                  uint32_t current_time_minutes) {
  cal.min_value = initial_reading;
  cal.max_value = initial_reading;
  cal.last_update_time = current_time_minutes;
}

bool AutoCal_update(AutoCal& cal, uint16_t new_reading,
                    uint32_t current_time_minutes, float alpha) {
  bool changed = false;

  // Convert reading to float for EMA operations
  float r = static_cast<float>(new_reading);

  // Ensure our float-backed state is primed. If it looks uninitialized
  // (equal to 0/1023 defaults), seed from the integer persisted values.
  if (cal.min_value_f == 0.0f && cal.max_value_f == 1023.0f &&
      !(cal.min_value == 0 && cal.max_value == 1023)) {
    cal.min_value_f = static_cast<float>(cal.min_value);
    cal.max_value_f = static_cast<float>(cal.max_value);
  }

  // Immediate expansion when reading outside current float-backed bounds
  if (r < cal.min_value_f) {
    cal.min_value_f = r;
    uint16_t newInt = static_cast<uint16_t>(roundf(cal.min_value_f));
    if (newInt != cal.min_value) {
      cal.min_value = newInt;
      changed = true;
    }
  } else {
    // Gradually forget old lows using EMA on the float state
    cal.min_value_f = cal.min_value_f + alpha * (r - cal.min_value_f);
    uint16_t newInt = static_cast<uint16_t>(roundf(cal.min_value_f));
    if (newInt != cal.min_value) {
      cal.min_value = newInt;
      changed = true;
    }
  }

  if (r > cal.max_value_f) {
    cal.max_value_f = r;
    uint16_t newInt = static_cast<uint16_t>(roundf(cal.max_value_f));
    if (newInt != cal.max_value) {
      cal.max_value = newInt;
      changed = true;
    }
  } else {
    // Gradually forget old highs using EMA on the float state
    cal.max_value_f = cal.max_value_f + alpha * (r - cal.max_value_f);
    uint16_t newInt = static_cast<uint16_t>(roundf(cal.max_value_f));
    if (newInt != cal.max_value) {
      cal.max_value = newInt;
      changed = true;
    }
  }

  // Always update timestamp (minutes since start)
  cal.last_update_time = current_time_minutes;

  // Ensure min <= max — in case of numerical issues, swap both float and int
  if (cal.min_value > cal.max_value) {
    // swap ints
    uint16_t t = cal.min_value;
    cal.min_value = cal.max_value;
    cal.max_value = t;
    // swap floats
    float tf = cal.min_value_f;
    cal.min_value_f = cal.max_value_f;
    cal.max_value_f = tf;
    changed = true;
  }

  return changed;
}

bool AutoCal_from_json(const JsonObject& obj, AutoCal& cal) {
  bool changed = false;
  if (!obj.isNull()) {
    if (obj["min_value"].is<float>() || obj["min_value"].is<int>() || obj["min_value"].is<unsigned int>()) {
      float v = obj["min_value"].as<float>();
      if (fabsf(v - cal.min_value) >= 0.001f) {
        cal.min_value = v;
        changed = true;
      }
    }
    if (obj["max_value"].is<float>() || obj["max_value"].is<int>() || obj["max_value"].is<unsigned int>()) {
      float v = obj["max_value"].as<float>();
      if (fabsf(v - cal.max_value) >= 0.001f) {
        cal.max_value = v;
        changed = true;
      }
    }
    if (obj["last_update_time"].is<unsigned long>()) {
      unsigned long t = obj["last_update_time"].as<unsigned long>();
      if (static_cast<uint32_t>(t) != cal.last_update_time) {
        cal.last_update_time = static_cast<uint32_t>(t);
        changed = true;
      }
    }
  }
  return changed;
}

void AutoCal_to_json(const AutoCal& cal, JsonObject& obj) {
  if (obj.isNull()) return;
  // Store float values directly — callers decide whether to round before
  // persisting to the public min/max fields in sensors.json to reduce
  // flash wear. The admin UI currently renders ints but can show float
  // autocal internals if needed.
  obj["min_value"] = cal.min_value;
  obj["max_value"] = cal.max_value;
  obj["last_update_time"] = cal.last_update_time;
}
