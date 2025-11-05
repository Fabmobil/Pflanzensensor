/**
 * @file web_ota_eeprom_backup.h
 * @brief EEPROM-based backup for OTA filesystem updates
 * @details Since vshymanskyy/Preferences uses LittleFS, all settings are lost
 *          during filesystem updates. This module uses ESP8266's native EEPROM
 *          (0x405F7000-0x405FB000, 16KB) to backup/restore critical settings.
 */

#pragma once

#include <Arduino.h>

// EEPROM Configuration
constexpr uint16_t EEPROM_SIZE = 4096;  // Use 4KB of the 16KB available
constexpr uint16_t EEPROM_MAGIC = 0xC0DE;  // Magic number to verify valid backup
constexpr uint8_t EEPROM_VERSION = 1;  // Backup format version

// EEPROM Layout
constexpr uint16_t EEPROM_HEADER_OFFSET = 0;
constexpr uint16_t EEPROM_GENERAL_OFFSET = 16;
constexpr uint16_t EEPROM_WIFI_OFFSET = 256;
constexpr uint16_t EEPROM_DISPLAY_OFFSET = 512;
constexpr uint16_t EEPROM_DEBUG_OFFSET = 640;
constexpr uint16_t EEPROM_LOG_OFFSET = 704;
constexpr uint16_t EEPROM_LED_OFFSET = 768;
constexpr uint16_t EEPROM_SENSORS_OFFSET = 1024;  // Start of sensor data

// Sensor configuration - support ANALOG (8 measurements) and DHT (2 measurements)
constexpr uint8_t MAX_SENSORS = 2;  // ANALOG and DHT
constexpr uint8_t MAX_MEASUREMENTS_ANALOG = 8;
constexpr uint8_t MAX_MEASUREMENTS_DHT = 2;
constexpr uint16_t SENSOR_DATA_SIZE = 512;  // Bytes per sensor

/**
 * @struct EEPROMBackupHeader
 * @brief Header for EEPROM backup to verify validity
 */
struct EEPROMBackupHeader {
  uint16_t magic;        // EEPROM_MAGIC to verify valid backup
  uint8_t version;       // Backup format version
  uint8_t flags;         // Bit flags for what's backed up
  uint32_t timestamp;    // Backup timestamp (millis)
  uint16_t checksum;     // Simple checksum of all data
  uint8_t reserved[6];   // Reserved for future use
};

/**
 * @struct EEPROMGeneralSettings
 * @brief General settings backup
 */
struct EEPROMGeneralSettings {
  char device_name[32];
  char admin_pwd[32];
  char flower_sens[16];
  bool md5_verify;
  bool collectd_en;
  bool file_log;
  uint8_t reserved[58];  // Padding to 144 bytes
};

/**
 * @struct EEPROMWiFiSettings
 * @brief WiFi credentials backup
 */
struct EEPROMWiFiSettings {
  char ssid1[32];
  char pwd1[64];
  char ssid2[32];
  char pwd2[64];
  char ssid3[32];
  char pwd3[64];
  uint8_t reserved[0];  // 288 bytes total
};

/**
 * @struct EEPROMDisplaySettings
 * @brief Display configuration backup
 */
struct EEPROMDisplaySettings {
  bool show_ip;
  bool show_clock;
  bool show_flower;
  bool show_fabmobil;
  uint32_t screen_dur;
  char clock_fmt[8];
  uint8_t reserved[48];  // Padding to 64 bytes
};

/**
 * @struct EEPROMDebugSettings
 * @brief Debug flags backup
 */
struct EEPROMDebugSettings {
  bool ram;
  bool meas_cycle;
  bool sensor;
  bool display;
  bool websocket;
  uint8_t reserved[59];  // Padding to 64 bytes
};

/**
 * @struct EEPROMLogSettings
 * @brief Logging configuration backup
 */
struct EEPROMLogSettings {
  uint8_t level;
  bool file_enabled;
  uint8_t reserved[62];  // Padding to 64 bytes
};

/**
 * @struct EEPROMLEDSettings
 * @brief LED traffic light configuration backup
 */
struct EEPROMLEDSettings {
  uint8_t mode;
  char sel_meas[32];
  uint8_t reserved[31];  // Padding to 64 bytes
};

/**
 * @struct EEPROMMeasurementConfig
 * @brief Configuration for a single measurement
 */
struct EEPROMMeasurementConfig {
  bool enabled;
  char name[24];
  char field_name[24];
  char unit[8];
  float min_value;
  float max_value;
  float yellow_low;
  float green_low;
  float green_high;
  float yellow_high;
  bool inverted;
  bool calibration_mode;
  uint32_t autocal_duration;
  int raw_min;
  int raw_max;
  uint8_t reserved[8];  // Padding to 128 bytes
};

/**
 * @struct EEPROMSensorConfig
 * @brief Configuration for a sensor
 */
struct EEPROMSensorConfig {
  bool initialized;
  char sensor_id[16];      // "ANALOG" or "DHT"
  char name[32];
  uint32_t meas_interval;
  bool has_error;
  uint8_t num_measurements;  // Actual number of measurements
  uint8_t reserved[10];
  EEPROMMeasurementConfig measurements[MAX_MEASUREMENTS_ANALOG];  // Max 8 measurements
};

/**
 * @class EEPROMBackup
 * @brief Handles EEPROM-based backup and restore for OTA updates
 */
class EEPROMBackup {
public:
  /**
   * @brief Initialize EEPROM for backup operations
   * @return true if EEPROM initialized successfully
   */
  static bool begin();

  /**
   * @brief Finalize EEPROM operations
   */
  static void end();

  /**
   * @brief Backup all settings to EEPROM
   * @return true if backup successful
   */
  static bool backupAllSettings();

  /**
   * @brief Restore all settings from EEPROM
   * @return true if restore successful
   */
  static bool restoreAllSettings();

  /**
   * @brief Check if valid backup exists in EEPROM
   * @return true if valid backup exists
   */
  static bool hasValidBackup();

  /**
   * @brief Clear EEPROM backup
   */
  static void clearBackup();

private:
  static bool backupGeneralSettings();
  static bool backupWiFiSettings();
  static bool backupDisplaySettings();
  static bool backupDebugSettings();
  static bool backupLogSettings();
  static bool backupLEDSettings();
  static bool backupSensorSettings();

  static bool restoreGeneralSettings();
  static bool restoreWiFiSettings();
  static bool restoreDisplaySettings();
  static bool restoreDebugSettings();
  static bool restoreLogSettings();
  static bool restoreLEDSettings();
  static bool restoreSensorSettings();

  static uint16_t calculateChecksum();
  static void writeHeader(bool valid);
};
