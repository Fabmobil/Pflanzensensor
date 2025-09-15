#include "sensors/sensors.h"

#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "sensors/sensor_count.h"

// REMOVE: getOrInitThresholds function and any references to it
// Instead, thresholds are now loaded directly into each sensor's measurement
// config during config load. If you need to initialize defaults, do so directly
// on the measurement config's limits member.

Sensor::Sensor(const SensorConfig& config, SensorManager* sensorManager)
    : m_sensorManager(sensorManager),
      m_id(config.id),
      m_measurementInterval(config.measurementInterval) {
  // Store config as our permanent configuration
  m_tempConfig = config;

  // Initialize measurement data arrays
  m_lastMeasurementData = std::make_unique<MeasurementData>();
  m_lastMeasurementData->activeValues = config.activeMeasurements;

  // Initialize statuses vector
  m_statuses.resize(config.activeMeasurements, "unknown");

  // Initialize state
  m_state.samples.resize(config.activeMeasurements);
  m_state.sampleCount = 0;
  m_state.measurementIndex = 0;
  m_state.sampleIndex = 0;
  m_state.measurementStarted = false;

  // Mark measurement data as valid since we just created it
  m_measurementDataValid = true;
}

Sensor::~Sensor() {
  m_lastMeasurementData.reset();
  m_measurementDataValid = false;
}

void Sensor::deinitialize() {
  m_initialized = false;
  // Don't deinitialize initial warmup sensors even if global setting is true
  if (isInitialWarmupSensor() && MEASUREMENT_DEINITIALIZE_SENSORS) {
    return;
  }
  // Mark MeasurementData as invalid but keep the pointer (no deallocation
  // needed)
  m_measurementDataValid = false;
  // DO NOT call m_lastMeasurementData.reset() here - it frees the memory!
  // We want to keep the memory allocated for potential re-initialization
  // Reset state info
  m_stateInfo = MeasurementStateInfo();
  m_isInWarmup = false;
  m_warmupStartTime = 0;
  m_errorState = ErrorState();
}

void Sensor::handleSensorError() {
  m_errorState.errorCount++;
  if (m_errorState.errorCount >= MAX_RETRIES) {
    logger.error(getName(), F(": Maximale Anzahl von Wiederholungen überschritten"));
    deinitialize();
  }
}

SensorResult Sensor::handleInvalidReading(float value) {
  m_errorState.invalidCount++;
  m_errorState.lastInvalidTime = millis();
  m_errorState.inRetryDelay = true;

  // Safe string conversion for NaN values
  String valueStr;
  if (isnan(value)) {
    valueStr = "NaN";
  } else {
    valueStr = String(value);
  }
  logger.error(getName(), F(": Ungültige Messung: ") + valueStr);

  if (m_errorState.invalidCount >= MAX_INVALID_READINGS) {
    logger.error(getName(),
                 F("Zu viele ungültige Messwerte, behandle als Sensorfehler"));
    handleSensorError();
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                              F("Zu viele ungültige Messwerte"));
  }

  if (isInRetryDelay()) {
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                              F("Still within retry delay period"));
  }

  m_errorState.inRetryDelay = false;
  return SensorResult::success();
}

void Sensor::stop() {
  m_enabled = false;
  deinitialize();
}

void Sensor::resetMeasurementState() {
  m_stateInfo = MeasurementStateInfo();
  m_isInWarmup = false;
  m_warmupStartTime = 0;
  m_errorState = ErrorState();  // Reset error state too
}

void Sensor::forceNextMeasurement() { m_stateInfo.lastMeasurementTime = 0; }

bool Sensor::isDueMeasurement() const {
  if (!m_enabled) return false;

  unsigned long now = millis();
  unsigned long timeSinceLastMeasurement =
      now - m_stateInfo.lastMeasurementTime;

  return timeSinceLastMeasurement >= m_measurementInterval;
}

bool Sensor::requiresWarmup(unsigned long& warmupTime) const {
  warmupTime = 0;
  return false;
}

SensorResult Sensor::startWarmup() {
  if (!requiresWarmup(m_warmupTime)) {
    return SensorResult::success();
  }

  m_isInWarmup = true;
  m_warmupStartTime = millis();
  return SensorResult::success();
}

bool Sensor::isWarmupComplete() const {
  if (!m_isInWarmup) {
    return true;
  }

  unsigned long now = millis();
  return (now - m_warmupStartTime) >= m_warmupTime;
}

bool Sensor::shouldDeinitializeAfterMeasurement() const {
  // Initial warmup sensors should never be deinitialized
  if (isInitialWarmupSensor()) {
    return false;
  }
  return MEASUREMENT_DEINITIALIZE_SENSORS;
}

const String& Sensor::getMeasurementName(size_t index) const {
  if (index < config().activeMeasurements) {
    return config().measurements[index].name;
  }
  static const String empty;
  return empty;
}

unsigned long Sensor::getMeasurementStartTime() const {
  return m_stateInfo.lastMeasurementTime;
}

SharedHardwareInfo Sensor::getSharedHardwareInfo() const {
  return SharedHardwareInfo(SensorType::UNKNOWN, 0, 0);
}

void Sensor::setState(MeasurementState newState) {
  m_stateInfo.state = newState;
}

void Sensor::updateLastMeasurementTime() {
  m_stateInfo.lastMeasurementTime = millis();
}

bool Sensor::shouldRetry() const {
  return m_errorState.errorCount < MAX_RETRIES;
}

SensorResult Sensor::validateMemoryState() const {
  // Check if measurement data is marked as valid
  if (!m_measurementDataValid) {
    logger.debug(
        getName(),
        F(": Measurement data marked as invalid, attempting recovery"));
    return SensorResult::fail(SensorError::RESOURCE_ERROR,
                              F("Measurement data invalid (deinitialized)"));
  }

  // Check if the measurement data pointer is valid (not null)
  if (!m_lastMeasurementData) {
    logger.error(getName(), F(": Null measurement data pointer"));
    return SensorResult::fail(SensorError::RESOURCE_ERROR,
                              F("Null measurement data pointer"));
  }

  // Check if measurement data structure is valid
  if (!m_lastMeasurementData->isValid()) {
    logger.error(getName(), F(": Invalid measurement data structure"));
    return SensorResult::fail(SensorError::RESOURCE_ERROR,
                              F("Invalid measurement data structure"));
  }

  // Check if measurement data arrays are properly sized
  if (SensorConfig::MAX_MEASUREMENTS != m_lastMeasurementData->values.size()) {
    logger.error(getName(), F(": Measurement data array size mismatch"));
    return SensorResult::fail(SensorError::RESOURCE_ERROR,
                              F("Measurement data array size mismatch"));
  }

  // Check if active values count is reasonable
  if (m_lastMeasurementData->activeValues >
      m_lastMeasurementData->values.size()) {
    logger.error(getName(), F(": Invalid active values count"));
    return SensorResult::fail(SensorError::RESOURCE_ERROR,
                              F("Invalid active values count"));
  }

  return SensorResult::success();
}

SensorResult Sensor::resetMemoryState() {
  logger.warning(getName(), F(": Attempting memory state reset"));

  // With pointer-based approach, we just reinitialize the existing data
  if (m_lastMeasurementData) {
    // Reinitialize the measurement data
    m_lastMeasurementData->activeValues = config().activeMeasurements;

    // Initialize all fields to empty first with bounds checking
    size_t maxFields = SensorConfig::MAX_MEASUREMENTS;
    maxFields = std::min(maxFields, m_lastMeasurementData->values.size());

    for (size_t i = 0; i < maxFields; i++) {
      // Don't clear field names and units - they should be preserved
      // Only clear values for fresh measurement
      m_lastMeasurementData->values[i] = 0.0f;
    }
  }

  // Mark as valid again
  m_measurementDataValid = true;

  // Validate the reset memory state
  auto validationResult = validateMemoryState();
  if (!validationResult.isSuccess()) {
    logger.error(getName(), F(": Zurücksetzen des Speicherzustands hat die Validierung nicht bestanden"));
    return validationResult;
  }

  logger.info(getName(), F(": Zurücksetzen des Speicherzustands erfolgreich"));
  return SensorResult::success();
}

void Sensor::initMeasurement(size_t index, const String& name,
                             const String& fieldName, const String& unit,
                             float yellowLow, float greenLow, float greenHigh,
                             float yellowHigh) {
  if (index >= SensorConfig::MAX_MEASUREMENTS) {
    logger.error(F("Sensor"),
                 F("initMeasurement: Index außerhalb des Bereichs: ") + String(index));
    return;
  }
  if (!m_lastMeasurementData || index >= SensorConfig::MAX_MEASUREMENTS) {
    logger.error(
        F("Sensor"),
        F("initMeasurement: Index außerhalb des MeasurementData-Array-Bereichs: ") +
            String(index));
    return;
  }
  mutableConfig().measurements[index].name = name;
  mutableConfig().measurements[index].fieldName =
      fieldName;  // <-- Ensure config has fieldName
  mutableConfig().measurements[index].unit =
      unit;  // <-- Ensure config has unit
  strncpy(m_lastMeasurementData->fieldNames[index], fieldName.c_str(),
          SensorConfig::FIELD_NAME_LEN - 1);
  m_lastMeasurementData->fieldNames[index][SensorConfig::FIELD_NAME_LEN - 1] =
      '\0';
  strncpy(m_lastMeasurementData->units[index], unit.c_str(),
          SensorConfig::UNIT_LEN - 1);
  m_lastMeasurementData->units[index][SensorConfig::UNIT_LEN - 1] = '\0';
  mutableConfig().measurements[index].limits.yellowLow = yellowLow;
  mutableConfig().measurements[index].limits.greenLow = greenLow;
  mutableConfig().measurements[index].limits.greenHigh = greenHigh;
  mutableConfig().measurements[index].limits.yellowHigh = yellowHigh;
}

void Sensor::updateStatus(size_t measurementIndex) {
  // **CRITICAL FIX: Add bounds checking for measurements array**
  if (measurementIndex >= config().activeMeasurements ||
      measurementIndex >= m_lastMeasurementData->activeValues ||
      measurementIndex >= config().measurements.size()) {
    // Ensure statuses vector is large enough
    if (measurementIndex >= m_statuses.size()) {
      m_statuses.resize(measurementIndex + 1, "unknown");
    }
    m_statuses[measurementIndex] = "unknown";
    return;
  }

  if (!m_lastMeasurementData || !m_lastMeasurementData->isValid()) {
    // Ensure statuses vector is large enough
    if (measurementIndex >= m_statuses.size()) {
      m_statuses.resize(measurementIndex + 1, "error");
    }
    m_statuses[measurementIndex] = "error";
    return;
  }

  // **CRITICAL FIX: Add bounds checking for values array**
  if (measurementIndex >= m_lastMeasurementData->values.size()) {
    // Ensure statuses vector is large enough
    if (measurementIndex >= m_statuses.size()) {
      m_statuses.resize(measurementIndex + 1, "error");
    }
    m_statuses[measurementIndex] = "error";
    return;
  }

  float measurementValue = m_lastMeasurementData->values[measurementIndex];
  const auto& limits = config().measurements[measurementIndex].limits;

  // Determine if this is a one-sided sensor
  bool isOneSided = false;
  if (getId().startsWith("SDS011") || getId().startsWith("MHZ19")) {
    isOneSided = true;  // PM and CO2 sensors use one-sided limits
  }

  // Ensure statuses vector is large enough
  if (measurementIndex >= m_statuses.size()) {
    m_statuses.resize(measurementIndex + 1, "unknown");
  }
  m_statuses[measurementIndex] =
      determineSensorStatus(measurementValue, limits, isOneSided);
}

const String& Sensor::getStatus(size_t measurementIndex) const {
  // Ensure statuses vector is large enough and return appropriate status
  if (measurementIndex >= m_statuses.size()) {
    // Return a static "unknown" string for out-of-bounds access
    static const String unknownStatus = "unknown";
    return unknownStatus;
  }
  return m_statuses[measurementIndex];
}

String Sensor::determineSensorStatus(float value, const SensorLimits& limits,
                                     bool isOneSided) {
  if (isOneSided) {
    // One-sided limits (like PM sensors): 0 to greenHigh is green, greenHigh to
    // yellowHigh is yellow, above yellowHigh is red
    if (value <= limits.greenHigh) {
      return "green";
    } else if (value <= limits.yellowHigh) {
      return "yellow";
    } else {
      return "red";
    }
  } else {
    // Two-sided limits: below yellowLow or above yellowHigh is red, between
    // yellowLow/greenLow and greenHigh/yellowHigh is yellow, between greenLow
    // and greenHigh is green
    if (value < limits.yellowLow || value > limits.yellowHigh) {
      return "red";
    } else if ((value >= limits.yellowLow && value < limits.greenLow) ||
               (value > limits.greenHigh && value <= limits.yellowHigh)) {
      return "yellow";
    } else {
      return "green";
    }
  }
}

void Sensor::updateMeasurementData(const MeasurementData& data) {
  if (!isInitialized()) {
    logger.error(getName(),
                 F(": updateMeasurementData called on uninitialized sensor!"));
    return;
  }
  if (!data.isValid()) {
    logger.error(getName(),
                 F(": updateMeasurementData called with invalid data!"));
    return;
  }
  // Defensive: Clamp activeValues to MAX_MEASUREMENTS
  MeasurementData safeData = data;
  if (safeData.activeValues > SensorConfig::MAX_MEASUREMENTS) {
    logger.error(getName(), F(": updateMeasurementData: activeValues > "
                              "MAX_MEASUREMENTS, clamping."));
    safeData.activeValues = SensorConfig::MAX_MEASUREMENTS;
  }
  *m_lastMeasurementData = safeData;
}

SensorResult Sensor::init() {
  if (!m_lastMeasurementData || !m_lastMeasurementData->isValid()) {
    m_lastMeasurementData = std::make_unique<MeasurementData>();
    m_lastMeasurementData->activeValues = config().activeMeasurements;
    // Clamp activeMeasurements and activeValues
    if (config().activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
      logger.warning(F("Sensor"), F("Clamping activeMeasurements from ") +
                                      String(config().activeMeasurements) +
                                      F(" to ") +
                                      String(SensorConfig::MAX_MEASUREMENTS));
      // Note: We can't modify the config directly anymore, but this is just a
      // warning
    }
    if (m_lastMeasurementData->activeValues > SensorConfig::MAX_MEASUREMENTS) {
      logger.warning(F("Sensor"),
                     F("Clamping activeValues from ") +
                         String(m_lastMeasurementData->activeValues) +
                         F(" to ") + String(SensorConfig::MAX_MEASUREMENTS));
      m_lastMeasurementData->activeValues = SensorConfig::MAX_MEASUREMENTS;
    }
    size_t maxFields = SensorConfig::MAX_MEASUREMENTS;
    maxFields = std::min(maxFields, m_lastMeasurementData->values.size());
    for (size_t i = 0; i < maxFields; i++) {
      m_lastMeasurementData->fieldNames[i][0] = '\0';
      m_lastMeasurementData->units[i][0] = '\0';
      m_lastMeasurementData->values[i] = 0.0f;
    }
  }
  m_measurementDataValid = true;
  m_initialized = true;
  return SensorResult::success();
}

/**
 * @brief Generic measurement loop (template method)
 * @details Handles sample collection, error handling, and averaging. Calls
 * fetchSample() for hardware access.
 */
SensorResult Sensor::performMeasurementCycle() {
  if (!isInitialized()) {
    logger.error(
        getName(),
        F(": performMeasurementCycle called on uninitialized sensor!"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Sensor not initialized"));
  }
  constexpr size_t NUM_SAMPLES = MEASUREMENT_AVERAGE_COUNT;  // Use config macro
  size_t numMeasurements = getNumMeasurements();

  // Defensive checks
  if (numMeasurements == 0) {
    logger.error(getName(), F(": getNumMeasurements() returned 0! This "
                              "indicates a configuration issue."));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("No measurements configured"));
  }

  if (numMeasurements > SensorConfig::MAX_MEASUREMENTS) {
    logger.error(getName(), F(": getNumMeasurements() returned more than "
                              "MAX_MEASUREMENTS! Clamping."));
    numMeasurements = SensorConfig::MAX_MEASUREMENTS;
  }

  if (ESP.getFreeHeap() < 2048) {  // Ensure we have at least 2KB free
    logger.error(getName(),
                 F(": Insufficient memory for measurement cycle. Free heap: ") +
                     String(ESP.getFreeHeap()));
    return SensorResult::fail(SensorError::MEMORY_ERROR,
                              F("Insufficient memory"));
  }

  // --- Nonblocking sample collection with minimumDelay ---
  if (!m_state.measurementStarted) {
    m_state.readInProgress = true;
    m_state.operationStartTime = millis();
    m_state.sampleCount = 0;
    m_state.samples.clear();
    try {
      m_state.samples.resize(numMeasurements);
    } catch (const std::exception& e) {
      logger.error(getName(),
                   F(": Failed to resize samples vector: ") + String(e.what()));
      m_state.readInProgress = false;
      return SensorResult::fail(SensorError::MEMORY_ERROR,
                                F("Failed to allocate measurement memory"));
    }
    m_state.measurementIndex = 0;
    m_state.sampleIndex = 0;
    m_state.measurementStarted = true;
    m_state.lastSampleTime = 0;
  }

  // Wait for minimumDelay between samples
  if (m_state.lastSampleTime != 0 &&
      (millis() - m_state.lastSampleTime < config().minimumDelay)) {
    return SensorResult::fail(SensorError::PENDING, "pending");
  }

  while (m_state.measurementIndex < numMeasurements) {
    while (m_state.sampleIndex < NUM_SAMPLES) {
      float value = 0.0f;
      if (!fetchSample(value, m_state.measurementIndex)) {
        handleInvalidReading(value);
        if (m_errorState.errorCount >= MAX_RETRIES) {
          m_state.readInProgress = false;
          m_state.measurementStarted = false;
          return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                                    F("Too many errors in measurement cycle"));
        }
        // Even on error, wait minimumDelay before next sample
        m_state.lastSampleTime = millis();
        return SensorResult::fail(SensorError::PENDING, "pending");
      }
      m_state.samples[m_state.measurementIndex].push_back(value);
      m_state.lastSampleTime = millis();
      m_state.sampleIndex++;
      // After each sample, return pending to allow nonblocking delay
      if (m_state.sampleIndex < NUM_SAMPLES) {
        return SensorResult::fail(SensorError::PENDING, "pending");
      }
    }
    m_state.sampleIndex = 0;
    m_state.measurementIndex++;
  }
  m_state.readInProgress = false;
  m_state.measurementStarted = false;
  // Defensive: If all samples are NaN, log and return error
  std::vector<float> averages = averageSamples();
  size_t validCount = 0;
  for (float v : averages) {
    if (!isnan(v)) validCount++;
  }
  if (validCount == 0) {
    logger.error(getName(), F(": All measurement results are invalid (NaN)"));
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                              F("All measurement results are invalid (NaN)"));
  }
  if (m_lastMeasurementData) {
    m_lastMeasurementData->activeValues = validCount;
  }
  return SensorResult::success();
}

/**
 * @brief Computes the average of the collected samples for each channel
 * @return Vector of averaged values (one per channel)
 */
std::vector<float> Sensor::averageSamples() const {
  std::vector<float> averages;
  for (const auto& channelSamples : m_state.samples) {
    if (channelSamples.empty()) {
      averages.push_back(NAN);
      continue;
    }
    float sum = 0.0f;
    size_t count = 0;
    for (float v : channelSamples) {
      if (!isnan(v)) {
        sum += v;
        ++count;
      }
    }
    averages.push_back(count > 0 ? sum / count : NAN);
  }
  return averages;
}

/**
 * @brief Returns the averaged results for each measurement channel
 * @return Vector of averaged values (one per channel)
 */
std::vector<float> Sensor::getAveragedResults() const {
  return averageSamples();
}

/**
 * @brief Helper to clear and shrink a std::vector (frees memory)
 * @tparam T Vector element type
 * @param vec Reference to the vector to clear and shrink
 */
template <typename T>
static void clearAndShrink(std::vector<T>& vec) {
  vec.clear();
  vec.shrink_to_fit();
}

// Implementation of config access methods that use the sensor manager
const SensorConfig& Sensor::config() const {
  // Always use our own config - the temporary config becomes permanent after
  // construction
  return m_tempConfig;
}

SensorConfig& Sensor::mutableConfig() {
  // Always use our own config - the temporary config becomes permanent after
  // construction
  return m_tempConfig;
}

const String& Sensor::getId() const { return m_id; }

const String& Sensor::getName() const { return config().name; }

void Sensor::setName(const String& name) { mutableConfig().name = name; }

size_t Sensor::getNumMeasurements() const {
  // **CRITICAL FIX: Ensure we never return 0 measurements**
  size_t count = config().activeMeasurements;
  if (count == 0) {
    logger.warning(getName(),
                   F(": getNumMeasurements() would return 0, using 1"));
    return 1;
  }
  return count;
}
