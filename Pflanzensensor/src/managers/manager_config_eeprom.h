/**
 * @file manager_config_eeprom.h
 * @brief EEPROM-based configuration storage (replacement for Preferences library)
 * @details Stores all configuration directly in ESP8266 EEPROM (0x405F7000, 16KB)
 *          which survives filesystem updates, eliminating the need for backup/restore.
 *          
 *          This replaces the vshymanskyy/Preferences library which uses LittleFS
 *          and gets wiped during OTA filesystem updates.
 */

#pragma once

#include <Arduino.h>
#include "utils/result_types.h"

// EEPROM Configuration
constexpr uint16_t CONFIG_EEPROM_SIZE = 16384;  // Use full 16KB EEPROM partition
constexpr uint16_t CONFIG_EEPROM_MAGIC = 0xCF19;  // Magic number to verify valid config
constexpr uint8_t CONFIG_EEPROM_VERSION = 1;  // Config format version

/**
 * @struct EEPROMConfigHeader
 * @brief Header for EEPROM configuration to verify validity
 */
struct EEPROMConfigHeader {
  uint16_t magic;        // CONFIG_EEPROM_MAGIC
  uint8_t version;       // CONFIG_EEPROM_VERSION
  uint8_t reserved;
  uint32_t write_count;  // Number of times config has been written (wear leveling info)
  uint16_t checksum;     // Checksum of all configuration data
  uint8_t padding[6];
} __attribute__((packed));

/**
 * @struct GeneralConfig
 * @brief General system configuration
 */
struct GeneralConfig {
  char device_name[32];
  char admin_pwd[64];
  char flower_sens[16];
  bool md5_verify;
  bool collectd_en;
  bool file_log;
  uint8_t reserved[49];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct WiFiConfig
 * @brief WiFi credentials and network configuration
 */
struct WiFiConfig {
  char ssid1[32];
  char pwd1[64];
  char ssid2[32];
  char pwd2[64];
  char ssid3[32];
  char pwd3[64];
  uint8_t reserved[32];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct DisplayConfig
 * @brief Display settings
 */
struct DisplayConfig {
  bool show_ip;
  bool show_clock;
  bool show_flower;
  bool show_fabmobil;
  uint32_t screen_dur;
  char clock_fmt[8];
  uint8_t reserved[52];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct DebugConfig
 * @brief Debug flags
 */
struct DebugConfig {
  bool ram;
  bool meas_cycle;
  bool sensor;
  bool display;
  bool websocket;
  uint8_t reserved[59];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct LogConfig
 * @brief Logging configuration
 */
struct LogConfig {
  uint8_t level;
  bool file_enabled;
  uint8_t reserved[62];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct LEDConfig
 * @brief LED traffic light configuration
 */
struct LEDConfig {
  uint8_t mode;
  char sel_meas[32];
  uint8_t reserved[31];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct MeasurementConfig
 * @brief Configuration for a single measurement
 */
struct MeasurementConfig {
  bool enabled;
  char name[32];
  char field_name[32];
  char unit[12];
  float min_value;
  float max_value;
  float yellow_low;
  float green_low;
  float green_high;
  float yellow_high;
  bool inverted;
  bool calibration_mode;
  uint32_t autocal_duration;
  int32_t raw_min;
  int32_t raw_max;
  uint8_t reserved[16];  // Padding for future expansion
} __attribute__((packed));

/**
 * @struct SensorConfig
 * @brief Configuration for a single sensor
 */
struct SensorConfig {
  bool initialized;
  char sensor_id[16];
  char name[32];
  uint32_t meas_interval;
  bool has_error;
  uint8_t num_measurements;
  uint8_t reserved[10];
  MeasurementConfig measurements[8];  // Max 8 measurements per sensor
} __attribute__((packed));

/**
 * @struct AllSensorsConfig
 * @brief Configuration for all sensors (ANALOG and DHT)
 */
struct AllSensorsConfig {
  SensorConfig analog;
  SensorConfig dht;
  uint8_t reserved[256];  // Reserved for future sensor types
} __attribute__((packed));

/**
 * @struct SystemConfig
 * @brief Complete system configuration stored in EEPROM
 */
struct SystemConfig {
  EEPROMConfigHeader header;
  GeneralConfig general;
  WiFiConfig wifi;
  DisplayConfig display;
  DebugConfig debug;
  LogConfig log;
  LEDConfig led;
  AllSensorsConfig sensors;
  uint8_t reserved[1024];  // Reserved for future expansion
} __attribute__((packed));

/**
 * @class EEPROMConfigStorage
 * @brief Manages configuration storage in EEPROM
 */
class EEPROMConfigStorage {
public:
  using ConfigResult = TypedResult<ConfigError, void>;

  /**
   * @brief Initialize EEPROM configuration storage
   * @return Result indicating success or failure
   */
  static ConfigResult begin();

  /**
   * @brief Check if valid configuration exists in EEPROM
   * @return true if valid configuration exists
   */
  static bool hasValidConfig();

  /**
   * @brief Initialize EEPROM with default configuration
   * @return Result indicating success or failure
   */
  static ConfigResult initializeDefaults();

  /**
   * @brief Load configuration from EEPROM into RAM
   * @return Result indicating success or failure
   */
  static ConfigResult load();

  /**
   * @brief Save configuration from RAM to EEPROM
   * @return Result indicating success or failure
   */
  static ConfigResult save();

  /**
   * @brief Get pointer to current configuration in RAM
   * @return Pointer to SystemConfig
   */
  static SystemConfig* getConfig();

  /**
   * @brief Verify EEPROM configuration checksum
   * @return true if checksum is valid
   */
  static bool verifyChecksum();

  /**
   * @brief Calculate checksum of current configuration
   * @return Calculated checksum value
   */
  static uint16_t calculateChecksum();

  /**
   * @brief Factory reset - clear all configuration
   * @return Result indicating success or failure
   */
  static ConfigResult factoryReset();

  /**
   * @brief Get write count (for wear leveling monitoring)
   * @return Number of times EEPROM has been written
   */
  static uint32_t getWriteCount();

private:
  static SystemConfig _config;  // Configuration cached in RAM
  static bool _initialized;
  static bool _loaded;

  static void loadDefaults();
  static ConfigResult writeToEEPROM();
  static ConfigResult readFromEEPROM();
};
