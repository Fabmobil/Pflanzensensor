#include "sensors/sensor_bmp280.h"

#if USE_BMP280

BMP280Sensor::~BMP280Sensor() = default;

BMP280Sensor::BMP280Sensor(const BMP280Config& config, SensorManager* sensorManager)
    : Sensor(config, sensorManager),
      m_bmp280Config(config),
      m_bmp280(std::make_unique<Adafruit_BMP280>()) {
  // Temperatur-Messung initialisieren
  mutableConfig().measurements[0].limits.yellowLow = BMP280_TEMPERATURE_YELLOW_LOW;
  mutableConfig().measurements[0].limits.greenLow = BMP280_TEMPERATURE_GREEN_LOW;
  mutableConfig().measurements[0].limits.greenHigh = BMP280_TEMPERATURE_GREEN_HIGH;
  mutableConfig().measurements[0].limits.yellowHigh = BMP280_TEMPERATURE_YELLOW_HIGH;
  initMeasurement(0, BMP280_TEMPERATURE_NAME, BMP280_TEMPERATURE_FIELD_NAME, F("°C"),
                  BMP280_TEMPERATURE_YELLOW_LOW, BMP280_TEMPERATURE_GREEN_LOW,
                  BMP280_TEMPERATURE_GREEN_HIGH, BMP280_TEMPERATURE_YELLOW_HIGH);
  // Luftdruck-Messung initialisieren
  mutableConfig().measurements[1].limits.yellowLow = BMP280_PRESSURE_YELLOW_LOW;
  mutableConfig().measurements[1].limits.greenLow = BMP280_PRESSURE_GREEN_LOW;
  mutableConfig().measurements[1].limits.greenHigh = BMP280_PRESSURE_GREEN_HIGH;
  mutableConfig().measurements[1].limits.yellowHigh = BMP280_PRESSURE_YELLOW_HIGH;
  initMeasurement(1, BMP280_PRESSURE_NAME, BMP280_PRESSURE_FIELD_NAME, F("hPa"),
                  BMP280_PRESSURE_YELLOW_LOW, BMP280_PRESSURE_GREEN_LOW, BMP280_PRESSURE_GREEN_HIGH,
                  BMP280_PRESSURE_YELLOW_HIGH);
}

void BMP280Sensor::logDebugDetails() const {
  logDebug(String(F("BMP280 I2C: 0x")) + String(BMP280_I2C_ADDRESS, HEX));
}

SensorResult BMP280Sensor::init() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;

  if (!m_bmp280->begin(BMP280_I2C_ADDRESS)) {
    logger.warning(getName(), F("BMP280 nicht gefunden (I2C 0x76)"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("BMP280 nicht gefunden"));
  }

  // Energiespar-Einstellungen für ESP8266
  m_bmp280->setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,   // Temperatur: 2× Überabtastung
                        Adafruit_BMP280::SAMPLING_X16,  // Druck: 16× Überabtastung
                        Adafruit_BMP280::FILTER_X16,    // IIR-Filter
                        Adafruit_BMP280::STANDBY_MS_500 // 500ms Standby
  );

  logger.info(getName(), F("BMP280 initialisiert"));
  return SensorResult::success();
}

SensorResult BMP280Sensor::startMeasurement() { return performMeasurementCycle(); }
SensorResult BMP280Sensor::continueMeasurement() { return performMeasurementCycle(); }

SharedHardwareInfo BMP280Sensor::getSharedHardwareInfo() const {
  return SharedHardwareInfo(SensorType::BMP280, bmp280Config().sckPin, bmp280Config().minimumDelay);
}

bool BMP280Sensor::fetchSample(float& value, size_t index) {
  if (!isInitialized())
    return false;
  if (index == 0) {
    value = m_bmp280->readTemperature();
  } else if (index == 1) {
    value = m_bmp280->readPressure() / 100.0f; // Pa → hPa
  } else {
    return false;
  }
  return validateReading(value, index == 0);
}

void BMP280Sensor::deinitialize() {
  Sensor::deinitialize();
  m_state = MeasurementState();
}

bool BMP280Sensor::validateReading(float value, bool isTemperature) const {
  if (isnan(value))
    return false;
  // Gültige Bereiche laut BMP280-Datenblatt
  if (isTemperature)
    return value >= -40.0f && value <= 85.0f;
  return value >= 300.0f && value <= 1100.0f;
}

bool BMP280Sensor::isValidValue(float value, size_t measurementIndex) const {
  return validateReading(value, measurementIndex == 0);
}

bool BMP280Sensor::canAccessHardware() const {
  return (millis() - m_state.lastHardwareAccess) >= bmp280Config().minimumDelay;
}

#endif // USE_BMP280
