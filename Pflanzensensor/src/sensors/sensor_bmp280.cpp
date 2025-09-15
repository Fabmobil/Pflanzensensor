#include "sensors/sensor_bmp280.h"

#if USE_BMP280

BMP280Sensor::~BMP280Sensor() {
  // Clean up sample vectors
  m_state.temperatureSamples.clear();
  m_state.pressureSamples.clear();
  // m_bmp280 will be automatically cleaned up by unique_ptr
}

BMP280Sensor::BMP280Sensor(const BMP280Config& config,
                           SensorManager* sensorManager)
    : Sensor(config, sensorManager),
      m_bmp280Config(config),
      m_bmp280(std::make_unique<Adafruit_BMP280>()) {
  // Initialize temperature measurement
  ThresholdDefaults tempDefaults = {
      BMP280_TEMPERATURE_YELLOW_LOW, BMP280_TEMPERATURE_GREEN_LOW,
      BMP280_TEMPERATURE_GREEN_HIGH, BMP280_TEMPERATURE_YELLOW_HIGH};
  mutableConfig().measurements[0].limits.yellowLow = tempDefaults.yellowLow;
  mutableConfig().measurements[0].limits.greenLow = tempDefaults.greenLow;
  mutableConfig().measurements[0].limits.greenHigh = tempDefaults.greenHigh;
  mutableConfig().measurements[0].limits.yellowHigh = tempDefaults.yellowHigh;
  initMeasurement(0, BMP280_TEMPERATURE_NAME, BMP280_TEMPERATURE_FIELD_NAME,
                  "°C", mutableConfig().measurements[0].limits.yellowLow,
                  mutableConfig().measurements[0].limits.greenLow,
                  mutableConfig().measurements[0].limits.greenHigh,
                  mutableConfig().measurements[0].limits.yellowHigh);

  // Initialize pressure measurement
  ThresholdDefaults pressDefaults = {
      BMP280_PRESSURE_YELLOW_LOW, BMP280_PRESSURE_GREEN_LOW,
      BMP280_PRESSURE_GREEN_HIGH, BMP280_PRESSURE_YELLOW_HIGH};
  mutableConfig().measurements[1].limits.yellowLow = pressDefaults.yellowLow;
  mutableConfig().measurements[1].limits.greenLow = pressDefaults.greenLow;
  mutableConfig().measurements[1].limits.greenHigh = pressDefaults.greenHigh;
  mutableConfig().measurements[1].limits.yellowHigh = pressDefaults.yellowHigh;
  initMeasurement(1, BMP280_PRESSURE_NAME, BMP280_PRESSURE_FIELD_NAME, "hPa",
                  mutableConfig().measurements[1].limits.yellowLow,
                  mutableConfig().measurements[1].limits.greenLow,
                  mutableConfig().measurements[1].limits.greenHigh,
                  mutableConfig().measurements[1].limits.yellowHigh);

  m_state.temperatureSamples.reserve(REQUIRED_SAMPLES);
  m_state.pressureSamples.reserve(REQUIRED_SAMPLES);
}

void BMP280Sensor::logDebugDetails() const {
  logDebug(F("BMP280-Konfig: ..."));  // Weitere Details bei Bedarf
}

SensorResult BMP280Sensor::init() {
  logDebug(F("Initialisiere BMP280-Sensor"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    logger.error(getName(), F("Speicher-Validierung fehlgeschlagen"));
    return memoryResult;
  }

  logger.debug(getName(), F("Beginne BMP280-Initialisierung"));

  if (!m_bmp280->begin(BMP280_I2C_ADDRESS)) {
    logger.error(getName(), F("BMP280-Sensor nicht gefunden"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("BMP280-Sensor nicht gefunden"));
  }

  // Set default sampling settings
  m_bmp280->setSampling(Adafruit_BMP280::MODE_NORMAL,   // Operating Mode
                        Adafruit_BMP280::SAMPLING_X2,   // Temp. oversampling
                        Adafruit_BMP280::SAMPLING_X16,  // Pressure oversampling
                        Adafruit_BMP280::FILTER_X16,    // Filtering
                        Adafruit_BMP280::STANDBY_MS_500);  // Standby time

  logger.debug(getName(), F("BMP280-Initialisierung erfolgreich"));
  return SensorResult::success();
}

/**
 * @brief Fetch a single sample for a given BMP280 measurement (0=temp,
 * 1=pressure)
 * @param value Reference to store the sample
 * @param index Measurement index
 * @return true if successful, false if hardware error
 */
bool BMP280Sensor::fetchSample(float& value, size_t index) {
  logDebug(F("Lese BMP280-Probe für Index ") + String(index));
  if (!isInitialized()) {
    logger.error(getName(),
                 F(": Versuch, Probe ohne Initialisierung zu lesen"));
    return false;
  }
  if (index == 0) {
    value = m_bmp280->readTemperature();
  } else if (index == 1) {
    value = m_bmp280->readPressure() / 100.0F;  // Convert Pa to hPa
  } else {
    value = NAN;
    return false;
  }
  logDebug(F("Gelesener Wert: ") + String(value));
  return !isnan(value);
}

void BMP280Sensor::deinitialize() {
  logDebug(F("Deinitialisiere BMP280-Sensor"));
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.temperatureSamples);
  Sensor::clearAndShrink(m_state.pressureSamples);
  m_state = MeasurementState();
}

bool BMP280Sensor::validateReading(float value, bool isTemperature) const {
  if (isnan(value)) {
    logger.error(getName(), F("Ungültige Messung (NaN)"));
    return false;
  }

  if (isTemperature) {
    // Temperature range for BMP280: -40 to +85°C
    if (value < -40.0f || value > 85.0f) {
      logger.error(getName(), F("Temperatur außerhalb des Bereichs: ") + String(value));
      return false;
    }
  } else {
    // Pressure range: 300 to 1100 hPa
    if (value < 300.0f || value > 1100.0f) {
      logger.error(getName(), F("Druck außerhalb des Bereichs: ") + String(value));
      return false;
    }
  }

  return true;
}

bool BMP280Sensor::isValidValue(float value, size_t measurementIndex) const {
  return validateReading(value, measurementIndex == 0);
}

bool BMP280Sensor::canAccessHardware() const {
  return (millis() - m_state.lastHardwareAccess) >= bmp280Config().minimumDelay;
}

#endif  // USE_BMP280
