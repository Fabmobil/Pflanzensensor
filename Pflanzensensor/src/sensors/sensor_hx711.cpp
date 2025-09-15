#if USE_HX711
#include "sensors/sensor_hx711.h"

HX711Sensor::~HX711Sensor() {
  // Clean up sample vector
  m_state.samples.clear();

  if (m_scale) {
    m_scale->power_down();  // Put the ADC in sleep mode
  }
}

HX711Sensor::HX711Sensor(const HX711Config& config,
                         SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_scale(std::make_unique<HX711>()) {
  // Initialize weight measurement
  ThresholdDefaults defaults = {HX711_YELLOW_LOW, HX711_GREEN_LOW,
                                HX711_GREEN_HIGH, HX711_YELLOW_HIGH};
  mutableConfig().measurements[0].limits.yellowLow = defaults.yellowLow;
  mutableConfig().measurements[0].limits.greenLow = defaults.greenLow;
  mutableConfig().measurements[0].limits.greenHigh = defaults.greenHigh;
  mutableConfig().measurements[0].limits.yellowHigh = defaults.yellowHigh;
  initMeasurement(
      0,                                                   // index
      HX711_NAME,                                          // name
      HX711_FIELD_NAME,                                    // field name
      HX711_UNIT,                                          // unit
      mutableConfig().measurements[0].limits.yellowLow,    // yellow low
      mutableConfig().measurements[0].limits.greenLow,     // green low
      mutableConfig().measurements[0].limits.greenHigh,    // green high
      mutableConfig().measurements[0].limits.yellowHigh);  // yellow high

  m_state.samples.reserve(REQUIRED_SAMPLES);
}

void HX711Sensor::logDebugDetails() const {
  logDebug(F("HX711-Konfig: ..."));  // Weitere Details bei Bedarf
}

SensorResult HX711Sensor::init() {
  logDebug(F("Initialisiere HX711-Sensor"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  logger.debug(getName(), F("Initialisiere HX711 an Pins DOUT:") +
                              String(hx711Config().doutPin) + F(" SCK:") +
                              String(hx711Config().sckPin));

  m_scale->begin(hx711Config().doutPin, hx711Config().sckPin);

  // Set scale and tare
  m_scale->set_scale(2280.f);  // TODO: Make this configurable
  m_scale->tare();

  delay(100);  // Sensor Zeit geben, sich einzuregulieren

  return SensorResult::success();
}

SensorResult HX711Sensor::startMeasurement() {
  logDebug(F("Starte HX711-Messung"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  m_state.reset();
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();

  return SensorResult::success();
}

SensorResult HX711Sensor::continueMeasurement() {
  logDebug(F("Setze HX711-Messung fort"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess() || !m_state.readInProgress) {
    return memoryResult;
  }

  // Check for timeout
  if (millis() - m_state.operationStartTime > 5000) {
    logger.error(getName(), F("Messzeitüberschreitung"));
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                              F("Messzeitüberschreitung"));
  }

  // Check if we can access hardware
  if (!canAccessHardware()) {
    return SensorResult::success();
  }

  // Read weight value
  float value;
  if (!readValue(value)) {
    auto handleResult = handleInvalidReading(value);
    if (!handleResult.isSuccess()) {
      return handleResult;  // Error limit reached
    }
    return SensorResult::success();  // Will handle retry delay internally
  }

  resetInvalidCount();  // Reset invalid counter on valid reading

  // CRITICAL: Add bounds checking before vector push_back
  if (m_state.sampleCount < REQUIRED_SAMPLES &&
      m_state.samples.size() < m_state.samples.capacity()) {
    m_state.samples.push_back(value);
    m_state.sampleCount++;
  } else {
    logger.warning(getName(), F("Probe-Buffer voll, überspringe Probe"));
  }

  m_state.lastHardwareAccess = millis();

  // Check if we have enough samples
  if (m_state.sampleCount >= REQUIRED_SAMPLES) {
    processResults();
    return SensorResult::fail(SensorError::SUCCESS, F("Messung abgeschlossen"));
  }

  return SensorResult::success();
}

void HX711Sensor::deinitialize() {
  logDebug(F("Deinitialisiere HX711-Sensor"));
  if (m_scale) {
    m_scale->power_down();
  }
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.samples);
  m_state = MeasurementState();
}

bool HX711Sensor::readValue(float& value) {
  if (!m_scale) {
    return false;
  }

  value = m_scale->get_units(1);  // Get one reading

  if (!validateReading(value)) {
    logger.error(getName(), F("Ungültige Messung: ") + String(value));
    return false;
  }

  logger.debug(getName(), F("Gewicht: ") + String(value) + F("g"));
  return true;
}

bool HX711Sensor::validateReading(float value) const {
  return !isnan(value) && value >= 0.0f;  // Weight cannot be negative
}

bool HX711Sensor::canAccessHardware() const {
  return (millis() - m_state.lastHardwareAccess) >= hx711Config().minimumDelay;
}

/**
 * @brief Fetch a single sample for the HX711 sensor (weight)
 * @param value Reference to store the sample
 * @param index Measurement index (should be 0)
 * @return true if successful, false if hardware error
 */
bool HX711Sensor::fetchSample(float& value, size_t index) {
  logDebug(F("Fetching HX711 sample for index ") + String(index));
  if (!isInitialized()) {
    logger.error(getName(),
                 F(": Attempted to fetch sample without initialization"));
    return false;
  }
  value = 0.0f;
  if (!readValue(value)) {
    return false;
  }
  logDebug(F("Fetched value: ") + String(value));
  return !isnan(value);
}
#endif  // USE_HX711
