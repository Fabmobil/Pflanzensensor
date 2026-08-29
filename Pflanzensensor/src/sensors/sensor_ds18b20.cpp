#include "sensors/sensor_ds18b20.h"

#if USE_DS18B20

DS18B20Sensor::~DS18B20Sensor() { Sensor::clearAndShrink(m_state.readings); }

DS18B20Sensor::DS18B20Sensor(const DS18B20Config& sensorConfig, SensorManager* sensorManager)
    : Sensor(sensorConfig, sensorManager),
      m_oneWireBus(sensorConfig.oneWireBus),
      m_sensorCount(sensorConfig.sensorCount),
      m_oneWire(std::make_unique<OneWire>(m_oneWireBus)),
      m_sensors(std::make_unique<DallasTemperature>(m_oneWire.get())) {
  // Statische Tabelle der Sensor-Defaults (bis zu 8 Sensoren)
  struct SensorDefaults {
    const char* name;
    const char* fieldName;
    float yellowLow, greenLow, greenHigh, yellowHigh;
  };
  static const SensorDefaults defaults[] = {
#if DS18B20_SENSOR_COUNT > 0
      {DS18B20_1_NAME, DS18B20_1_FIELD_NAME, DS18B20_1_YELLOW_LOW, DS18B20_1_GREEN_LOW,
       DS18B20_1_GREEN_HIGH, DS18B20_1_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 1
      {DS18B20_2_NAME, DS18B20_2_FIELD_NAME, DS18B20_2_YELLOW_LOW, DS18B20_2_GREEN_LOW,
       DS18B20_2_GREEN_HIGH, DS18B20_2_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 2
      {DS18B20_3_NAME, DS18B20_3_FIELD_NAME, DS18B20_3_YELLOW_LOW, DS18B20_3_GREEN_LOW,
       DS18B20_3_GREEN_HIGH, DS18B20_3_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 3
      {DS18B20_4_NAME, DS18B20_4_FIELD_NAME, DS18B20_4_YELLOW_LOW, DS18B20_4_GREEN_LOW,
       DS18B20_4_GREEN_HIGH, DS18B20_4_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 4
      {DS18B20_5_NAME, DS18B20_5_FIELD_NAME, DS18B20_5_YELLOW_LOW, DS18B20_5_GREEN_LOW,
       DS18B20_5_GREEN_HIGH, DS18B20_5_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 5
      {DS18B20_6_NAME, DS18B20_6_FIELD_NAME, DS18B20_6_YELLOW_LOW, DS18B20_6_GREEN_LOW,
       DS18B20_6_GREEN_HIGH, DS18B20_6_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 6
      {DS18B20_7_NAME, DS18B20_7_FIELD_NAME, DS18B20_7_YELLOW_LOW, DS18B20_7_GREEN_LOW,
       DS18B20_7_GREEN_HIGH, DS18B20_7_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 7
      {DS18B20_8_NAME, DS18B20_8_FIELD_NAME, DS18B20_8_YELLOW_LOW, DS18B20_8_GREEN_LOW,
       DS18B20_8_GREEN_HIGH, DS18B20_8_YELLOW_HIGH},
#endif
  };
  static constexpr size_t DEFAULT_COUNT = sizeof(defaults) / sizeof(defaults[0]);

  for (size_t i = 0; i < m_sensorCount && i < SensorConfig::MAX_MEASUREMENTS; i++) {
    const SensorDefaults& d =
        (i < DEFAULT_COUNT) ? defaults[i] : SensorDefaults{"DS18B20", "ds18b20", 0, 0, 40, 60};
    mutableConfig().measurements[i].limits.yellowLow = d.yellowLow;
    mutableConfig().measurements[i].limits.greenLow = d.greenLow;
    mutableConfig().measurements[i].limits.greenHigh = d.greenHigh;
    mutableConfig().measurements[i].limits.yellowHigh = d.yellowHigh;
    initMeasurement(i, d.name, d.fieldName, "°C", d.yellowLow, d.greenLow, d.greenHigh,
                    d.yellowHigh);
  }
  m_state.readings.resize(m_sensorCount, 0.0f);
}

void DS18B20Sensor::logDebugDetails() const {
  logDebug(String(F("DS18B20: pin=")) + String(m_oneWireBus) + String(F(" count=")) +
           String(m_sensorCount));
}

SensorResult DS18B20Sensor::init() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;

  pinMode(m_oneWireBus, INPUT_PULLUP);
  m_sensors->begin();
  // Kein blocking delay – DS18B20 benötigt nach begin() keinen festen Delay

  uint8_t found = m_sensors->getDeviceCount();
  if (found == 0) {
    LOG_WARN(getName(), F("Keine DS18B20 Sensoren gefunden"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Kein Sensor gefunden"));
  }

  if (found < m_sensorCount) {
    LOG_WARN(getName(), String(F("Erwartet ")) + String(m_sensorCount) + String(F(", gefunden ")) +
                            String(found));
    // Konfiguration an tatsächliche Anzahl anpassen
    mutableConfig().activeMeasurements = found;
    if (m_lastMeasurementData)
      m_lastMeasurementData->activeValues = found;
    m_state.readings.resize(found, 0.0f);
    Sensor::m_state.samples.resize(found);
    m_statuses.resize(found, "unknown");
  }

  // Auflösung auf 10 Bit (187ms Konversionszeit) – spart Zeit
  m_sensors->setResolution(10);
  LOG_INFO(getName(), String(F("DS18B20 initialisiert: ")) + String(found) +
                          String(F(" Sensor(en) an Pin ")) + String(m_oneWireBus));
  m_initialized = true;
  return SensorResult::success();
}

SensorResult DS18B20Sensor::startMeasurement() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;
  m_state.reset(config().activeMeasurements);
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  if (!requestTemperatures()) {
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR, F("Konversion fehlgeschlagen"));
  }
  return SensorResult::success();
}

SensorResult DS18B20Sensor::continueMeasurement() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess() || !m_state.readInProgress)
    return memResult;

  if (millis() - m_state.operationStartTime > 5000UL) {
    LOG_ERROR(getName(), F("Messung Timeout"));
    handleSensorError();
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR, F("Timeout"));
  }
  // Warten bis Konversion abgeschlossen (non-blocking)
  if (!m_sensors->isConversionComplete())
    return SensorResult::success();
  return SensorResult::success();
}

SensorResult DS18B20Sensor::performMeasurementCycle() {
  if (!isInitialized()) {
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Nicht initialisiert"));
  }
  size_t numSensors = getNumMeasurements();
  if (numSensors == 0) {
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("0 Messungen"));
  }

  // Zustand initialisieren
  if (!cycleConversionInProgress && cycleMeasurementIndex == 0) {
    m_state.readings.resize(numSensors, 0.0f);
  }

  while (cycleMeasurementIndex < numSensors) {
    if (!cycleConversionInProgress) {
      // Konversion für alle Sensoren auf dem Bus starten
      if (!requestTemperatures()) {
        return SensorResult::fail(SensorError::MEASUREMENT_ERROR, F("Konversion fehlgeschlagen"));
      }
      cycleConversionStart = millis();
      cycleConversionInProgress = true;
      return SensorResult::fail(SensorError::PENDING, "pending");
    }
    // Warten auf Konversionszeit (10-bit = ~187ms)
    if (millis() - cycleConversionStart < MAX_CONVERSION_TIME) {
      return SensorResult::fail(SensorError::PENDING, "pending");
    }
    // Wert lesen
    float value = m_sensors->getTempCByIndex(cycleMeasurementIndex);
    m_state.readings[cycleMeasurementIndex] = value;
    cycleMeasurementIndex++;
    cycleConversionInProgress = false;
    if (cycleMeasurementIndex < numSensors) {
      return SensorResult::fail(SensorError::PENDING, "pending");
    }
  }

  // Alle Sensoren ausgelesen – in Basis-Samples übertragen
  cycleMeasurementIndex = 0;
  Sensor::m_state.samples.resize(m_state.readings.size());
  for (size_t i = 0; i < m_state.readings.size(); ++i) {
    Sensor::m_state.samples[i].clear();
    Sensor::m_state.samples[i].push_back(m_state.readings[i]);
  }
  return SensorResult::success();
}

void DS18B20Sensor::deinitialize() {
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.readings);
  m_state.reset(config().activeMeasurements);
}

bool DS18B20Sensor::validateReading(float value) const {
  if (isnan(value) || value == -127.0f || value == DEVICE_DISCONNECTED_C)
    return false;
  return value >= -55.0f && value <= 125.0f;
}

bool DS18B20Sensor::canAccessHardware() const {
  return (millis() - m_state.lastHardwareAccess) >= ds18b20Config().minimumDelay;
}

bool DS18B20Sensor::requestTemperatures() {
  if (!m_sensors)
    return false;
  m_sensors->requestTemperatures();
  m_state.conversionRequested = true;
  m_state.lastHardwareAccess = millis();
  return true;
}

bool DS18B20Sensor::fetchSample(float& value, size_t index) {
  if (!isInitialized())
    return false;
  value = m_sensors->getTempCByIndex(index);
  return validateReading(value);
}

size_t DS18B20Sensor::getNumMeasurements() const { return config().activeMeasurements; }

#endif // USE_DS18B20
