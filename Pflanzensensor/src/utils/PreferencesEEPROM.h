/**
 * @file PreferencesEEPROM.h
 * @brief Drop-in replacement for Preferences library that uses EEPROM instead of LittleFS
 * @details This class provides the same API as the vshymanskyy/Preferences library but
 *          stores data in ESP8266 EEPROM (0x405F7000, 16KB) which survives filesystem updates.
 *          
 *          Simply replace `#include <Preferences.h>` with `#include "PreferencesEEPROM.h"`
 *          and change `Preferences prefs` to `PreferencesEEPROM prefs`.
 */

#pragma once

#include <Arduino.h>
#include <EEPROM.h>

// EEPROM configuration  
#define PREFS_EEPROM_SIZE 4096  // Use 4KB of 16KB available EEPROM
#define PREFS_MAGIC 0x5052      // "PR" for Preferences
#define PREFS_VERSION 1

// Maximum sizes
#define MAX_NAMESPACES 32
#define MAX_KEY_LENGTH 15
#define MAX_STRING_LENGTH 64
#define NAMESPACE_NAME_LENGTH 15
#define NAMESPACE_DATA_SIZE 128  // Bytes per namespace

// EEPROM layout offsets
#define EEPROM_HEADER_OFFSET 0
#define EEPROM_HEADER_SIZE 16
#define EEPROM_DIR_OFFSET EEPROM_HEADER_SIZE
#define EEPROM_DIR_SIZE (MAX_NAMESPACES * sizeof(NamespaceEntry))
#define EEPROM_DATA_OFFSET (EEPROM_DIR_OFFSET + EEPROM_DIR_SIZE)

/**
 * @struct NamespaceEntry
 * @brief Directory entry for a namespace in EEPROM
 */
struct NamespaceEntry {
  char name[NAMESPACE_NAME_LENGTH + 1];
  uint16_t offset;          // Offset in EEPROM data area
  uint16_t size;            // Allocated size
  uint8_t initialized;      // 1 if namespace exists
  uint8_t reserved[7];
} __attribute__((packed));

/**
 * @class PreferencesEEPROM
 * @brief Drop-in replacement for Preferences using EEPROM storage
 */
class PreferencesEEPROM {
public:
  PreferencesEEPROM();
  ~PreferencesEEPROM();

  /**
   * @brief Open a namespace
   * @param name Namespace name (max 15 characters)
   * @param readOnly true for read-only access
   * @return true if successful
   */
  bool begin(const char* name, bool readOnly = false);

  /**
   * @brief Close the current namespace
   */
  void end();

  /**
   * @brief Clear all keys in the current namespace
   * @return true if successful
   */
  bool clear();

  /**
   * @brief Remove a specific key
   * @param key Key name
   * @return true if successful
   */
  bool remove(const char* key);

  // Getters
  String getString(const char* key, const String& defaultValue = String());
  bool getBool(const char* key, bool defaultValue = false);
  uint8_t getUChar(const char* key, uint8_t defaultValue = 0);
  uint16_t getUShort(const char* key, uint16_t defaultValue = 0);
  uint32_t getUInt(const char* key, uint32_t defaultValue = 0);
  int8_t getChar(const char* key, int8_t defaultValue = 0);
  int16_t getShort(const char* key, int16_t defaultValue = 0);
  int32_t getInt(const char* key, int32_t defaultValue = 0);
  float getFloat(const char* key, float defaultValue = 0.0f);

  // Setters (return bytes written, 0 on failure)
  size_t putString(const char* key, const String& value);
  size_t putBool(const char* key, bool value);
  size_t putUChar(const char* key, uint8_t value);
  size_t putUShort(const char* key, uint16_t value);
  size_t putUInt(const char* key, uint32_t value);
  size_t putChar(const char* key, int8_t value);
  size_t putShort(const char* key, int16_t value);
  size_t putInt(const char* key, int32_t value);
  size_t putFloat(const char* key, float value);

  /**
   * @brief Check if a key exists
   * @param key Key name
   * @return true if key exists
   */
  bool isKey(const char* key);

  /**
   * @brief Initialize EEPROM storage
   * @return true if successful
   */
  static bool initializeStorage();

private:
  char _namespace[NAMESPACE_NAME_LENGTH + 1];
  bool _readOnly;
  int _namespaceIndex;
  uint16_t _dataOffset;

  int findNamespace(const char* name);
  int createNamespace(const char* name);
  uint16_t makeKey(const char* key);
  bool writeValue(const char* key, const void* value, size_t len);
  bool readValue(const char* key, void* value, size_t len);

  static bool _initialized;
};

// Type alias for compatibility
using Preferences = PreferencesEEPROM;
