#include "sensor_dht.h"

#include <DHTesp.h>

#include "configs/config.h"
#include "managers/manager_config.h"
#include "sensors/sensors.h"

// File-local DHTesp instance for DHTSensor
namespace {
DHTesp m_dhtesp;
}

// DHTSensor constructor now stores type from config
DHTSensor::DHTSensor(const DHTConfig& config, SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_pin(config.pin), m_type(DHT_TYPE) {
  // Use config as source of truth for thresholds, writing macro defaults if
  // missing
  ThresholdDefaults tempDefaults = {DHT_TEMPERATURE_YELLOW_LOW, DHT_TEMPERATURE_GREEN_LOW,
                                    DHT_TEMPERATURE_GREEN_HIGH, DHT_TEMPERATURE_YELLOW_HIGH};
  mutableConfig().measurements[0].limits.yellowLow = tempDefaults.yellowLow;
  mutableConfig().measurements[0].limits.greenLow = tempDefaults.greenLow;
  mutableConfig().measurements[0].limits.greenHigh = tempDefaults.greenHigh;
  mutableConfig().measurements[0].limits.yellowHigh = tempDefaults.yellowHigh;
  initMeasurement(0, DHT_TEMPERATURE_NAME, DHT_TEMPERATURE_FIELD_NAME, DHT_TEMPERATURE_UNIT,
                  mutableConfig().measurements[0].limits.yellowLow,
                  mutableConfig().measurements[0].limits.greenLow,
                  mutableConfig().measurements[0].limits.greenHigh,
                  mutableConfig().measurements[0].limits.yellowHigh);

  ThresholdDefaults humDefaults = {DHT_HUMIDITY_YELLOW_LOW, DHT_HUMIDITY_GREEN_LOW,
                                   DHT_HUMIDITY_GREEN_HIGH, DHT_HUMIDITY_YELLOW_HIGH};
  mutableConfig().measurements[1].limits.yellowLow = humDefaults.yellowLow;
  mutableConfig().measurements[1].limits.greenLow = humDefaults.greenLow;
  mutableConfig().measurements[1].limits.greenHigh = humDefaults.greenHigh;
  mutableConfig().measurements[1].limits.yellowHigh = humDefaults.yellowHigh;
  initMeasurement(1, DHT_HUMIDITY_NAME, DHT_HUMIDITY_FIELD_NAME, DHT_HUMIDITY_UNIT,
                  mutableConfig().measurements[1].limits.yellowLow,
                  mutableConfig().measurements[1].limits.greenLow,
                  mutableConfig().measurements[1].limits.greenHigh,
                  mutableConfig().measurements[1].limits.yellowHigh);
}

DHTSensor::~DHTSensor() {
  // No explicit cleanup needed for DHTesp
}

void DHTSensor::logDebugDetails() const {
  logDebug(F("DHT-Konfig: pin=") + String(m_pin) + F(", typ=") + String(m_type));
}

SensorResult DHTSensor::init() {
  logDebug(F("Initialisiere DHT-Sensor an Pin ") + String(m_pin));
  DHTesp::DHT_MODEL_t dhtModel = (m_type == 22) ? DHTesp::DHT22 : DHTesp::DHT11;
  m_dhtesp.setup(m_pin, dhtModel);
  m_initialized = true;
  logger.debug(getName(), F("DHTesp-Initialisierung abgeschlossen (Typ: ") +
                              String((m_type == 22) ? "DHT22" : "DHT11") + F(")"));
  return SensorResult::success();
}

/**
 * @brief Returns the averaged results for DHTSensor (temperature and humidity)
 * @return Vector of averaged values (temp, humidity)
 */
// [REMOVED: std::vector<float> DHTSensor::readMeasurement()]

void DHTSensor::deinitialize() {
  logDebug(F("Deinitialisiere DHT-Sensor"));
  Sensor::deinitialize();
  m_initialized = false;

  // Memory cleanup
  ESP.wdtFeed();
  yield();
}

bool DHTSensor::requiresWarmup(unsigned long& warmupTime) const {
  warmupTime = 1000; // DHT sensors need 1 second between readings (reduced from 2000ms)
  return true;
}

bool DHTSensor::canAccessHardware() const {
  // Always return true for DHTesp (no status check needed)
  unsigned long now = millis();
  return (now - m_state.lastHardwareAccess) >= HARDWARE_ACCESS_DELAY_MS;
}

/**
 * @brief Fetch a single sample for a given measurement index (0=temp,
 * 1=humidity)
 * @param value Reference to store the sample
 * @param index Measurement index
 * @return true if successful, false if hardware error
 */
bool DHTSensor::fetchSample(float& value, size_t index) {
  logDebug(F("Lese DHT-Probe für Index ") + String(index));
  if (!m_initialized) {
    logger.error(getName(), F("DHTSensor nicht in fetchSample initialisiert"));
    return false;
  }
  if (index == 0) {
    value = m_dhtesp.getTemperature();
  } else if (index == 1) {
    value = m_dhtesp.getHumidity();
  } else {
    value = NAN;
    return false;
  }
  logDebug(F("Gelesener Wert: ") + String(value));
  return !isnan(value);
}

SensorResult DHTSensor::startMeasurement() {
  logDebug(F("Starting DHT measurement"));
  return performMeasurementCycle();
}

SensorResult DHTSensor::continueMeasurement() {
  logDebug(F("Continuing DHT measurement"));
  return performMeasurementCycle();
}

bool DHTSensor::isValidValue(float value, size_t measurementIndex) const {
  return measurementIndex == 0 ? isValidTemperature(value) : isValidHumidity(value);
}

SharedHardwareInfo DHTSensor::getSharedHardwareInfo() const {
  return SharedHardwareInfo(SensorType::DHT, m_pin, config().minimumDelay);
}
