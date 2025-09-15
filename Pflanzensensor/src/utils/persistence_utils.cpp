/**
 * @file persistence_utils.cpp
 * @brief Implementation of shared utilities for persistence operations
 */

#include "persistence_utils.h"

#include <LittleFS.h>

#include "logger/logger.h"

namespace PersistenceUtils {

void logFileContents(const String& tag, const String& contents,
                     size_t chunkSize) {
  size_t len = contents.length();
  for (size_t i = 0; i < len; i += chunkSize) {
    size_t endPos = (i + chunkSize < len) ? i + chunkSize : len;
    logger.debug(tag, contents.substring(i, endPos));
  }
}

bool readJsonFile(const char* path, ArduinoJson::JsonDocument& doc,
                  String& errorMsg) {
  if (!LittleFS.begin()) {
    errorMsg = F("Einbinden des Dateisystems fehlgeschlagen");
    return false;
  }
  if (!fileExists(path)) {
    errorMsg = F("Datei existiert nicht: ") + String(path);
    return false;
  }
  File file = LittleFS.open(path, "r");
  if (!file) {
    errorMsg = F("Öffnen der Datei zum Lesen fehlgeschlagen: ") + String(path);
    return false;
  }
  // Deserialize directly from file stream to save RAM
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    errorMsg = F("JSON-Parsefehler: ") + String(error.c_str());
    return false;
  }
  return true;
}

bool writeJsonFile(const char* path, const ArduinoJson::JsonDocument& doc,
                   String& errorMsg) {
  // Write to a temporary file first for atomicity
  String tempPath = String(path) + F(".tmp");
  File file = LittleFS.open(tempPath.c_str(), "w");
  if (!file) {
    errorMsg = F("Öffnen der temporären Datei zum Schreiben fehlgeschlagen: ") + tempPath;
    return false;
  }
  size_t written = serializeJson(doc, file);
  file.close();
  if (written == 0) {
    errorMsg = F("Schreiben des JSON in die temporäre Datei fehlgeschlagen: ") + tempPath;
    LittleFS.remove(tempPath.c_str());
    return false;
  }
  // Remove the original file before renaming (if it exists)
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
  }
  // Rename temp file to final path
  if (!LittleFS.rename(tempPath.c_str(), path)) {
    errorMsg = F("Umbenennen der temporären Datei in den endgültigen Pfad fehlgeschlagen: ") + String(path);
    LittleFS.remove(tempPath.c_str());
    return false;
  }
  return true;
}

}  // namespace PersistenceUtils
