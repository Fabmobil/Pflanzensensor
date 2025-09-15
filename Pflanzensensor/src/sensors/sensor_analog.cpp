#include "sensors/sensor_analog.h"

#if USE_ANALOG
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"

AnalogSensor::~AnalogSensor() { m_state.samples.clear(); }

AnalogSensor::AnalogSensor(const AnalogConfig& config,
                           SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_analogConfig(config) {
  struct SensorDefaults {
    const char* name;
    const char* fieldName;
    float yellowLow;
    float greenLow;
    float greenHigh;
    float yellowHigh;
    float min;
    float max;
    bool inverted;
  };

  static const SensorDefaults analogDefaults[] = {
#if ANALOG_SENSOR_COUNT > 0
    {ANALOG_1_NAME, ANALOG_1_FIELD_NAME, ANALOG_1_YELLOW_LOW,
     ANALOG_1_GREEN_LOW, ANALOG_1_GREEN_HIGH, ANALOG_1_YELLOW_HIGH,
     ANALOG_1_MIN, ANALOG_1_MAX, ANALOG_1_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 1
    {ANALOG_2_NAME, ANALOG_2_FIELD_NAME, ANALOG_2_YELLOW_LOW,
     ANALOG_2_GREEN_LOW, ANALOG_2_GREEN_HIGH, ANALOG_2_YELLOW_HIGH,
     ANALOG_2_MIN, ANALOG_2_MAX, ANALOG_2_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 2
    {ANALOG_3_NAME, ANALOG_3_FIELD_NAME, ANALOG_3_YELLOW_LOW,
     ANALOG_3_GREEN_LOW, ANALOG_3_GREEN_HIGH, ANALOG_3_YELLOW_HIGH,
     ANALOG_3_MIN, ANALOG_3_MAX, ANALOG_3_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 3
    {ANALOG_4_NAME, ANALOG_4_FIELD_NAME, ANALOG_4_YELLOW_LOW,
     ANALOG_4_GREEN_LOW, ANALOG_4_GREEN_HIGH, ANALOG_4_YELLOW_HIGH,
     ANALOG_4_MIN, ANALOG_4_MAX, ANALOG_4_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 4
    {ANALOG_5_NAME, ANALOG_5_FIELD_NAME, ANALOG_5_YELLOW_LOW,
     ANALOG_5_GREEN_LOW, ANALOG_5_GREEN_HIGH, ANALOG_5_YELLOW_HIGH,
     ANALOG_5_MIN, ANALOG_5_MAX, ANALOG_5_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 5
    {ANALOG_6_NAME, ANALOG_6_FIELD_NAME, ANALOG_6_YELLOW_LOW,
     ANALOG_6_GREEN_LOW, ANALOG_6_GREEN_HIGH, ANALOG_6_YELLOW_HIGH,
     ANALOG_6_MIN, ANALOG_6_MAX, ANALOG_6_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 6
    {ANALOG_7_NAME, ANALOG_7_FIELD_NAME, ANALOG_7_YELLOW_LOW,
     ANALOG_7_GREEN_LOW, ANALOG_7_GREEN_HIGH, ANALOG_7_YELLOW_HIGH,
     ANALOG_7_MIN, ANALOG_7_MAX, ANALOG_7_INVERTED},
#endif
#if ANALOG_SENSOR_COUNT > 7
    {ANALOG_8_NAME, ANALOG_8_FIELD_NAME, ANALOG_8_YELLOW_LOW,
     ANALOG_8_GREEN_LOW, ANALOG_8_GREEN_HIGH, ANALOG_8_YELLOW_HIGH,
     ANALOG_8_MIN, ANALOG_8_MAX, ANALOG_8_INVERTED},
#endif
  };

  size_t maxChannels = sizeof(analogDefaults) / sizeof(analogDefaults[0]);
  if (m_analogConfig.activeMeasurements > maxChannels) {
    logger.warning(getName(), F("Begrenze activeMeasurements von ") +
                                  String(m_analogConfig.activeMeasurements) +
                                  F(" auf ") + String(maxChannels));
    m_analogConfig.activeMeasurements = maxChannels;
  }
  m_lastRawValues.clear();
  for (size_t i = 0; i < m_analogConfig.activeMeasurements; ++i) {
    const auto& def = analogDefaults[i];
    auto& meas = m_analogConfig.measurements[i];
    meas.minValue = def.min;
    meas.maxValue = def.max;
    meas.inverted = def.inverted;
    m_lastRawValues.push_back(-1);  // Initialize with -1 (invalid)
    initMeasurement(i, def.name, def.fieldName, "%", def.yellowLow,
                    def.greenLow, def.greenHigh, def.yellowHigh);
  }
#if USE_MULTIPLEXER
  if (config.useMultiplexer) {
    m_multiplexer = std::make_unique<Multiplexer>();
  }
#endif
}

void AnalogSensor::logDebugDetails() const {
  logDebug(F("Analog-Konfig: pin=") + String(m_analogConfig.pin) +
           F(", activeMeasurements=") +
           String(m_analogConfig.activeMeasurements));
}

SensorResult AnalogSensor::init() {
  logDebug(F("Initialisiere Analog-Sensor an Pin ") +
           String(m_analogConfig.pin));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }
  m_state.samples.clear();
#if USE_MULTIPLEXER
  if (m_analogConfig.useMultiplexer) {
    if (!m_multiplexer) {
      m_multiplexer = std::make_unique<Multiplexer>();
    }
    auto muxResult = m_multiplexer->init();
    if (!muxResult.isSuccess()) {
      logger.error(getName(), F(": Multiplexer-Initialisierung fehlgeschlagen"));
      return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                                F("Multiplexer-Initialisierung fehlgeschlagen"));
    }
  }
#endif
  pinMode(m_analogConfig.pin, INPUT);
  logger.debug(getName(),
               F(": Initialisiert an Pin ") + String(m_analogConfig.pin));
  m_initialized = true;
  return SensorResult::success();
}

SensorResult AnalogSensor::startMeasurement() {
  logDebug(F("Starte Analogmessung"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }
  if (m_analogConfig.activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
    logger.warning(getName(), F("Begrenze activeMeasurements von ") +
                                  String(m_analogConfig.activeMeasurements) +
                                  F(" auf ") +
                                  String(SensorConfig::MAX_MEASUREMENTS));
    m_analogConfig.activeMeasurements = SensorConfig::MAX_MEASUREMENTS;
  }
  if (!isInitialized()) {
    logger.error(getName(),
                 F(": Versuch, Messung ohne Initialisierung zu starten"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Sensor nicht initialisiert"));
  }
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  logger.debug(getName(), F(": Starting new measurement cycle for ") +
                              String(m_analogConfig.activeMeasurements) +
                              F(" sensors"));
  return SensorResult::success();
}

SensorResult AnalogSensor::continueMeasurement() {
  logDebug(F("Setze Analogmessung fort"));
  // DRY: The base class handles measurement cycling, so just validate state and
  // return success
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess() || !m_state.readInProgress) {
    return memoryResult;
  }
  if (!isInitialized()) {
    logger.error(
        getName(),
        F(": Versuch, Messung fortzusetzen ohne Initialisierung"));
    m_state.readInProgress = false;
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Sensor nicht initialisiert"));
  }
  if (millis() - m_state.operationStartTime > 5000) {  // Hardcoded timeout
  logger.error(getName(), F(": Messzeitüberschreitung nach ") +
                String(millis() - m_state.operationStartTime) +
                F("ms"));
    m_state.readInProgress = false;
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                              F("Measurement timeout"));
  }
  if (!canAccessHardware()) {
    return SensorResult::success();
  }
  // All actual measurement is handled by the base class via fetchSample
  return SensorResult::success();
}

void AnalogSensor::deinitialize() {
  logDebug(F("Deinitializing analog sensor"));
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.samples);
#if USE_MULTIPLEXER
  if (m_multiplexer) {
    m_multiplexer.reset();
  }
#endif
}

bool AnalogSensor::validateReading(int reading, size_t measurementIndex) const {
  if (measurementIndex >= m_analogConfig.measurements.size()) {
    logDebug(F("AnalogSensor: Index out of bounds for measurements! index=") +
             String(measurementIndex));
    return false;
  }
  // For analog sensors, we now accept all readings since we clamp them in
  // fetchSample This method is kept for compatibility but always returns true
  // for valid indices
  return true;
}

float AnalogSensor::mapAnalogValue(int rawValue,
                                   size_t measurementIndex) const {
  if (measurementIndex >= m_analogConfig.measurements.size()) {
    logDebug(F("AnalogSensor: Index out of bounds for measurements! index=") +
             String(measurementIndex));
    return 0.0f;
  }
  float minValue = m_analogConfig.measurements[measurementIndex].minValue;
  float maxValue = m_analogConfig.measurements[measurementIndex].maxValue;
  bool inverted = m_analogConfig.measurements[measurementIndex].inverted;

  if (maxValue == minValue) return 0.0f;

  // If inverted, we need to map the raw value as if the range was flipped
  // For inverted sensors: high raw value should give low percentage
  if (inverted) {
    // Map from [minValue, maxValue] to [100%, 0%] instead of [0%, 100%]
    float percentage = 100.0f * (maxValue - rawValue) / (maxValue - minValue);
    logDebug(F("Inverted mapping: raw=") + String(rawValue) + F(", min=") +
             String(minValue) + F(", max=") + String(maxValue) +
             F(", result=") + String(percentage) + F("%"));
    return percentage;
  } else {
    // Normal mapping from [minValue, maxValue] to [0%, 100%]
    float percentage = 100.0f * (rawValue - minValue) / (maxValue - minValue);
    logDebug(F("Normal mapping: raw=") + String(rawValue) + F(", min=") +
             String(minValue) + F(", max=") + String(maxValue) +
             F(", result=") + String(percentage) + F("%"));
    return percentage;
  }
}

bool AnalogSensor::fetchSample(float& value, size_t index) {
  logDebug(F("Fetching analog sample for index ") + String(index));
#if USE_MULTIPLEXER
  if (m_analogConfig.useMultiplexer && m_multiplexer) {
    if (!m_multiplexer->switchToSensor(index + 1)) {
      logger.error(getName(),
                   F(": Failed to select channel ") + String(index + 1));
      value = NAN;
      return false;
    }
    delay(5);  // Small settling time for multiplexer
  }
#endif
  if (index >= m_analogConfig.measurements.size()) {
    logDebug(F("AnalogSensor: Index out of bounds for measurements! index=") +
             String(index));
    value = NAN;
    return false;
  }
  int raw = analogRead(m_analogConfig.pin);
  // Store last raw value
  if (index < m_lastRawValues.size()) {
    m_lastRawValues[index] = raw;
  }

  // Track absolute raw min/max values
  if (index < m_analogConfig.measurements.size()) {
    auto& measurement = m_analogConfig.measurements[index];
    bool minMaxChanged = false;

    // Store original values for comparison
    int originalMin = measurement.absoluteRawMin;
    int originalMax = measurement.absoluteRawMax;

    // CRITICAL FIX: Only update if we have a genuine new minimum or maximum
    // This prevents overwriting existing min/max values with current readings
    if (raw < measurement.absoluteRawMin) {
      measurement.absoluteRawMin = raw;
      minMaxChanged = true;
      if (ConfigMgr.isDebugSensor()) {
        logger.debug(getName(), F("New minimum found: ") + String(raw) +
                                    F(" (was: ") + String(originalMin) +
                                    F(")"));
      }
    }
    if (raw > measurement.absoluteRawMax) {
      measurement.absoluteRawMax = raw;
      minMaxChanged = true;
      if (ConfigMgr.isDebugSensor()) {
        logger.debug(getName(), F("New maximum found: ") + String(raw) +
                                    F(" (was: ") + String(originalMax) +
                                    F(")"));
      }
    }

    // Only trigger persistence update if min/max values actually changed
    if (minMaxChanged) {
      if (ConfigMgr.isDebugSensor()) {
        logger.debug(getName(), F("Raw min/max changed - Min: ") +
                                    String(measurement.absoluteRawMin) +
                                    F(", Max: ") +
                                    String(measurement.absoluteRawMax) +
                                    F(" for index ") + String(index));
      }

      auto result = SensorPersistence::updateAnalogRawMinMax(
          m_analogConfig.id, index, measurement.absoluteRawMin,
          measurement.absoluteRawMax);

      if (ConfigMgr.isDebugSensor()) {
        if (result.isSuccess()) {
          logger.debug(getName(),
                       F("Successfully persisted raw min/max update"));
        } else {
          logger.error(getName(), F("Failed to persist raw min/max update: ") +
                                      result.getMessage());
        }
      }
    }
  }

  // Check if raw value is out of range and clamp it instead of treating as
  // error
  float minValue = m_analogConfig.measurements[index].minValue;
  float maxValue = m_analogConfig.measurements[index].maxValue;
  int clampedRaw = raw;
  bool wasClamped = false;

  if (raw < static_cast<int>(minValue)) {
    clampedRaw = static_cast<int>(minValue);
    wasClamped = true;
  } else if (raw > static_cast<int>(maxValue)) {
    clampedRaw = static_cast<int>(maxValue);
    wasClamped = true;
  }

  // Log warning if value was clamped
  if (wasClamped) {
    logger.warning(getName(),
                   F(": Raw value out of range: ") + String(raw) + F(" (min=") +
                       String(minValue) + F(", max=") + String(maxValue) +
                       F("), using clamped value: ") + String(clampedRaw));
  }

  // Use clamped value for mapping
  value = mapAnalogValue(clampedRaw,
                         index);  // Map to percentage or calibrated value

  // Debug logging for inverted sensors
  if (index < m_analogConfig.measurements.size() &&
      m_analogConfig.measurements[index].inverted) {
    logDebug(F("Inverted sensor: raw=") + String(clampedRaw) + F(", mapped=") +
             String(value) + F("%"));
  }

  logDebug(F("Fetched value: ") + String(value));
  return !isnan(value);
}

bool AnalogSensor::canAccessHardware() const {
  static unsigned long lastAccess = 0;
  unsigned long now = millis();
  if (now - lastAccess >= m_analogConfig.minimumDelay) {
    lastAccess = now;
    return true;
  }
  return false;
}

#endif  // USE_ANALOG
