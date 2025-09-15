/**
 * @file persistence_utils.h
 * @brief Shared utilities for persistence operations (file I/O, logging, JSON)
 *
 * Provides helper functions for checking file existence, getting file size,
 * logging file contents in chunks, and reading/writing JSON files.
 */
#ifndef PERSISTENCE_UTILS_H
#define PERSISTENCE_UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

/**
 * @namespace PersistenceUtils
 * @brief Utility functions for persistence operations
 */
namespace PersistenceUtils {

/**
 * @brief Check if a file exists
 * @param path Path to the file
 * @return true if file exists, false otherwise
 */
inline bool fileExists(const char* path) { return LittleFS.exists(path); }

/**
 * @brief Get the size of a file
 * @param path Path to the file
 * @return Size in bytes, or 0 if file does not exist or cannot be opened
 */
inline size_t getFileSize(const char* path) {
  if (!fileExists(path)) {
    return 0;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    return 0;
  }
  size_t size = f.size();
  f.close();
  return size;
}

/**
 * @brief Log file contents in chunks to avoid memory issues
 * @param tag Tag for the logger
 * @param contents File contents to log
 * @param chunkSize Size of each chunk (default: 100)
 */
void logFileContents(const String& tag, const String& contents,
                     size_t chunkSize = 100);

/**
 * @brief Read and deserialize a JSON file
 * @param path Path to the file
 * @param doc Reference to a JsonDocument to populate
 * @param errorMsg String to receive error message if any
 * @return true if successful, false otherwise
 */
bool readJsonFile(const char* path, ArduinoJson::JsonDocument& doc,
                  String& errorMsg);

/**
 * @brief Serialize and write a JSON document to file
 * @param path Path to the file
 * @param doc Reference to a JsonDocument to write
 * @param errorMsg String to receive error message if any
 * @return true if successful, false otherwise
 */
bool writeJsonFile(const char* path, const ArduinoJson::JsonDocument& doc,
                   String& errorMsg);

}  // namespace PersistenceUtils

#endif  // PERSISTENCE_UTILS_H
