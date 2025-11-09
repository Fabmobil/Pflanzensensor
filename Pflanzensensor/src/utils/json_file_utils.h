/**
 * @file json_file_utils.h
 * @brief Utility functions for reading/writing small JSON files with minimal RAM
 */

#ifndef JSON_FILE_UTILS_H
#define JSON_FILE_UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

/**
 * @brief Load a small JSON file into a DynamicJsonDocument
 * @param path File path to load
 * @param doc Pre-allocated JsonDocument to fill
 * @return true if successful, false otherwise
 */
inline bool loadJsonFile(const String& path, DynamicJsonDocument& doc) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();

  return error == DeserializationError::Ok;
}

/**
 * @brief Save a JsonDocument to a file atomically (using .tmp + rename)
 * @param path File path to write
 * @param doc JsonDocument to save
 * @return true if successful, false otherwise
 */
inline bool saveJsonFile(const String& path, const JsonDocument& doc) {
  String tmpPath = path + ".tmp";

  // Write to temporary file first
  File file = LittleFS.open(tmpPath, "w");
  if (!file) {
    return false;
  }

  size_t bytesWritten = serializeJson(doc, file);
  file.close();

  if (bytesWritten == 0) {
    LittleFS.remove(tmpPath);
    return false;
  }

  // Atomic rename
  LittleFS.remove(path); // Remove old file if exists
  return LittleFS.rename(tmpPath, path);
}

/**
 * @brief Build measurement file path
 * @param sensorId Sensor ID (e.g., "ANALOG", "DHT")
 * @param index Measurement index (0-based)
 * @return Full path like "/config/sensor_ANALOG_0.json"
 */
inline String getMeasurementFilePath(const String& sensorId, size_t index) {
  return "/config/sensor_" + sensorId + "_" + String(index) + ".json";
}

#endif // JSON_FILE_UTILS_H
