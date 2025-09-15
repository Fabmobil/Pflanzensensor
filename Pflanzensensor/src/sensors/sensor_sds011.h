/**
 * @file sensor_sds011.h
 * @brief SDS011 sensor class for particulate matter measurement
 */

#ifndef SENSOR_SDS011_H
#define SENSOR_SDS011_H

#include <SoftwareSerial.h>

#include "sensors/sensors.h"

#if USE_SDS011

// Protocol constants
static constexpr uint8_t SDS011_HEAD = 0xAA;
static constexpr uint8_t SDS011_TAIL = 0xAB;
static constexpr uint8_t SDS011_CMD_ID = 0xB4;
static constexpr uint8_t SDS011_REPORT_ID = 0xC0;
static constexpr uint8_t SDS011_QUERY_CMD = 0x04;
static constexpr uint8_t SDS011_SLEEP_CMD = 0x06;
static constexpr uint8_t SDS011_SET_ID = 0x05;
static constexpr uint8_t SDS011_RESPONSE_ID = 0xC5;

// Command constants
static constexpr uint8_t SDS011_WORK_MODE = 0x01;
static constexpr uint8_t SDS011_SLEEP_MODE = 0x00;

// Timing constants
static constexpr unsigned long SDS011_COMMAND_TIMEOUT =
    1000;                                                // 1 second timeout
static constexpr unsigned long SDS011_RETRY_DELAY = 10;  // 10ms between retries
static constexpr unsigned long SDS011_MAX_RETRIES = 3;

// Response lengths
static constexpr size_t SDS011_RESPONSE_LENGTH = 10;
static constexpr size_t SDS011_COMMAND_LENGTH = 19;

struct SDS011Config : public SensorConfig {
  uint8_t rxPin;
  uint8_t txPin;
  unsigned long warmupTime;

  /**
   * @brief Default constructor for SDS011Config
   * @details Sets id to 'SDS011'.
   */
  SDS011Config()
      : rxPin(SDS011_RX_PIN),
        txPin(SDS011_TX_PIN),
        warmupTime(SDS011_WARMUP_TIME) {
    name = F("SDS011");
    id = F("SDS011");        // Unified ID
    activeMeasurements = 2;  // PM10 and PM2.5
    if (measurementInterval == 0)
      measurementInterval = SDS011_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = SDS011_MINIMUM_DELAY;
  }
};

enum class SDS011Status {
  Ok,
  NotAvailable,
  InvalidChecksum,
  InvalidResponseId,
  InvalidHead,
  InvalidTail,
  Error
};

class SDS011Sensor : public Sensor {
 public:
  /**
   * @brief Constructor for SDS011Sensor
   * @param config Configuration structure for the sensor
   */
  explicit SDS011Sensor(const SDS011Config& config,
                        class SensorManager* sensorManager);
  /**
   * @brief Destructor
   * @override
   */
  ~SDS011Sensor() override;

  /**
   * @brief Initialize the sensor
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult init() override;
  /**
   * @brief Start a new measurement cycle
   * @return SensorResult indicating success or failure with error details
   * @override
   */
  SensorResult startMeasurement() override;
  /**
   * @brief Continue ongoing measurement
   * @return SensorResult indicating success, failure, or measurement completion
   * @override
   */
  SensorResult continueMeasurement() override;
  /**
   * @brief Perform a complete measurement cycle with proper sleep/wake handling
   * @return SensorResult indicating success, failure, or measurement completion
   * @override
   */
  SensorResult performMeasurementCycle() override;
  /**
   * @brief Test basic communication with the sensor
   * @return true if communication is working, false otherwise
   */
  bool testCommunication();

  /**
   * @brief Deinitialize the sensor
   * @override
   */
  void deinitialize() override;

  /**
   * @brief Check if a value is within valid range
   * @param value The measurement value to check
   * @return true if value is valid, false otherwise
   * @override
   */
  bool isValidValue(float value) const override {
    return !isnan(value) && value > 0.0f && value < 1000.0f;
  }

  /**
   * @brief Check if a value is valid for a specific measurement index
   * @param value The measurement value to check
   * @param measurementIndex Index of the measurement
   * @return true if value is valid, false otherwise
   * @override
   */
  bool isValidValue(float value, size_t measurementIndex) const override {
    return isValidValue(value);
  }

  /**
   * @brief Get warmup requirements
   * @param warmupTime Reference to store required warmup time
   * @return true if sensor requires warmup, false if kept awake
   * @override
   */
  bool requiresWarmup(unsigned long& warmupTime) const override;

  /**
   * @brief Check if sensor needs warmup before measurement
   * @return true if this is a warmup sensor and not kept awake
   * @override
   */
  bool isMeasurementWarmupSensor() const override {
    return true;  // Always requires warmup, no per-sensor keepAwake flag
  }

  /**
   * @brief Get shared hardware information
   * @return SharedHardwareInfo structure with sensor details
   * @override
   */
  SharedHardwareInfo getSharedHardwareInfo() const override {
    return SharedHardwareInfo(SensorType::SDS011, sds011Config().rxPin,
                              sds011Config().minimumDelay);
  }

  static constexpr size_t REQUIRED_SAMPLES = 3;
  static constexpr size_t MAX_SAMPLES = 5;

  /**
   * @brief Fetch a single sample for a given SDS011 measurement (0=PM10,
   * 1=PM2.5)
   * @param value Reference to store the sample
   * @param index Measurement index
   * @return true if successful, false if hardware error
   */
  bool fetchSample(float& value, size_t index) override;
  /**
   * @brief Log sensor-specific debug details
   */
  void logDebugDetails() const override;

 protected:
  /**
   * @brief Returns the number of measurements for SDS011Sensor (2: PM10 and
   * PM2.5)
   * @return Number of measurements (always 2)
   */
  inline size_t getNumMeasurements() const override { return 2; }

 private:
  const uint8_t m_rxPin;
  const uint8_t m_txPin;
  const unsigned long m_warmupTime;
  std::unique_ptr<SoftwareSerial> m_serial;

  struct {
    float pm10[MAX_SAMPLES];
    float pm25[MAX_SAMPLES];
    uint8_t count{0};
    uint8_t invalidCount{0};
    unsigned long lastAccess{0};
    unsigned long startTime{0};
    bool inProgress{false};
    bool wakingUp{false};
    bool sleeping{true};
  } m_state;

  uint8_t m_command[SDS011_COMMAND_LENGTH];
  uint8_t m_response[SDS011_RESPONSE_LENGTH];

  float m_lastPM10{0.0f};
  float m_lastPM25{0.0f};

  // Helper methods
  const SDS011Config& sds011Config() const {
    return static_cast<const SDS011Config&>(config());
  }

  bool sendCommand(uint8_t cmd, uint8_t data1 = 0x01, uint8_t data2 = 0x01);
  SDS011Status readResponse(unsigned long timeout = SDS011_COMMAND_TIMEOUT);
  bool readValues();
  float calculateAverage(const float* samples, size_t count) const;
  void processResults();
  bool wakeup();
  bool sleep();
  void handleSensorError();
  uint8_t calculateChecksum(const uint8_t* data, size_t length) const;
  void prepareCommand(uint8_t cmd, uint8_t data1, uint8_t data2);
};

#endif  // USE_SDS011
#endif  // SENSOR_SDS011_H
