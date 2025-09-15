#include "sensors/sensor_ds18b20.h"

#if USE_DS18B20

// In sensor_ds18b20.cpp

DS18B20Sensor::~DS18B20Sensor() {
  // Clean up readings vector
  m_state.readings.clear();
  // m_oneWire and m_sensors will be automatically cleaned up by unique_ptr
}

DS18B20Sensor::DS18B20Sensor(const DS18B20Config& sensorConfig,
                             SensorManager* sensorManager)
    : Sensor(sensorConfig, sensorManager),
      m_oneWireBus(sensorConfig.oneWireBus),
      m_sensorCount(sensorConfig.sensorCount),
      m_oneWire(std::make_unique<OneWire>(m_oneWireBus)),
      m_sensors(std::make_unique<DallasTemperature>(m_oneWire.get())) {
  // Define a struct to hold all config values for one sensor
  struct SensorDefaults {
    const char* name;
    const char* fieldName;
    float yellowLow;
    float greenLow;
    float greenHigh;
    float yellowHigh;
  };

  // Create array of sensor defaults using preprocessor checks
  const SensorDefaults defaults[] = {
#if DS18B20_SENSOR_COUNT > 0
    {DS18B20_1_NAME, DS18B20_1_FIELD_NAME, DS18B20_1_YELLOW_LOW,
     DS18B20_1_GREEN_LOW, DS18B20_1_GREEN_HIGH, DS18B20_1_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 1
    {DS18B20_2_NAME, DS18B20_2_FIELD_NAME, DS18B20_2_YELLOW_LOW,
     DS18B20_2_GREEN_LOW, DS18B20_2_GREEN_HIGH, DS18B20_2_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 2
    {DS18B20_3_NAME, DS18B20_3_FIELD_NAME, DS18B20_3_YELLOW_LOW,
     DS18B20_3_GREEN_LOW, DS18B20_3_GREEN_HIGH, DS18B20_3_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 3
    {DS18B20_4_NAME, DS18B20_4_FIELD_NAME, DS18B20_4_YELLOW_LOW,
     DS18B20_4_GREEN_LOW, DS18B20_4_GREEN_HIGH, DS18B20_4_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 4
    {DS18B20_5_NAME, DS18B20_5_FIELD_NAME, DS18B20_5_YELLOW_LOW,
     DS18B20_5_GREEN_LOW, DS18B20_5_GREEN_HIGH, DS18B20_5_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 5
    {DS18B20_6_NAME, DS18B20_6_FIELD_NAME, DS18B20_6_YELLOW_LOW,
     DS18B20_6_GREEN_LOW, DS18B20_6_GREEN_HIGH, DS18B20_6_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 6
    {DS18B20_7_NAME, DS18B20_7_FIELD_NAME, DS18B20_7_YELLOW_LOW,
     DS18B20_7_GREEN_LOW, DS18B20_7_GREEN_HIGH, DS18B20_7_YELLOW_HIGH},
#endif
#if DS18B20_SENSOR_COUNT > 7
    {DS18B20_8_NAME, DS18B20_8_FIELD_NAME, DS18B20_8_YELLOW_LOW,
     DS18B20_8_GREEN_LOW, DS18B20_8_GREEN_HIGH, DS18B20_8_YELLOW_HIGH},
#endif
  };

  // Initialize measurements first
  if (config().activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
    logger.warning(getName(), F("Begrenze activeMeasurements von ") +
                                  String(config().activeMeasurements) +
                                  F(" auf ") +
                                  String(SensorConfig::MAX_MEASUREMENTS));
    // Note: We can't modify the config directly anymore, but this is just a
    // warning
  }
  for (size_t i = 0; i < m_sensorCount; i++) {
    const auto& def =
        (i < sizeof(defaults) / sizeof(defaults[0]))
            ? defaults[i]
            : SensorDefaults{"DS18B20_unknown", "ds18b20_unknown", 0, 0, 0, 0};
    ThresholdDefaults tdef = {def.yellowLow, def.greenLow, def.greenHigh,
                              def.yellowHigh};
    mutableConfig().measurements[i].limits.yellowLow = tdef.yellowLow;
    mutableConfig().measurements[i].limits.greenLow = tdef.greenLow;
    mutableConfig().measurements[i].limits.greenHigh = tdef.greenHigh;
    mutableConfig().measurements[i].limits.yellowHigh = tdef.yellowHigh;
    initMeasurement(i, def.name, def.fieldName, "°C", tdef.yellowLow,
                    tdef.greenLow, tdef.greenHigh, tdef.yellowHigh);
  }

  // Initialize readings vector to correct size
  m_state.readings.resize(config().activeMeasurements, 0.0f);
}

void DS18B20Sensor::logDebugDetails() const {
  logDebug(F("DS18B20-Konfig: bus=") + String(m_oneWireBus) +
           F(", sensorCount=") + String(m_sensorCount));
}

SensorResult DS18B20Sensor::init() {
  logDebug(F("Initialisiere DS18B20-Sensor am Bus ") + String(m_oneWireBus));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  logger.debug(getName(), F("DS18B20 init: verwende Pin ") + String(m_oneWireBus));
  pinMode(m_oneWireBus, INPUT_PULLUP);
  int pinStateBefore = digitalRead(m_oneWireBus);
  logger.debug(getName(), F("DS18B20 init: Pin-Zustand vor begin: ") +
                              String(pinStateBefore));

  m_sensors->begin();
  delay(100);  // Brief delay for initialization

  int pinStateAfter = digitalRead(m_oneWireBus);
  logger.debug(getName(), F("DS18B20 init: pin state after begin: ") +
                              String(pinStateAfter));

  // Print all found sensor addresses
  uint8_t deviceCount = m_sensors->getDeviceCount();
  logger.info(getName(), F("Gefunden ") + String(deviceCount) +
                             F(" DS18B20-Geräte. Adressenliste:"));
  DeviceAddress addr;
  for (uint8_t i = 0; i < deviceCount; ++i) {
    if (m_sensors->getAddress(addr, i)) {
      String addressStr;
      for (uint8_t j = 0; j < 8; ++j) {
        if (j > 0) addressStr += ":";
        addressStr += String(addr[j], HEX);
      }
  logger.info(getName(), F("Sensor-Index ") + String(i) + F(" Adresse: ") +
             addressStr);
    } else {
      logger.warning(getName(),
                     F("Konnte Adresse für Sensor Index ") + String(i) + F(" nicht lesen"));
    }
  }

  logger.debug(getName(), F("DS18B20 init: getDeviceCount() lieferte ") +
                              String(deviceCount));
  if (deviceCount < m_sensorCount) {
    // Log the discrepancy but continue with reduced sensor count
  logger.warning(getName(), F("Erwartet ") + String(m_sensorCount) +
                  F(" Sensoren, aber nur ") +
                  String(deviceCount) +
                  F(" gefunden. Führe mit reduzierter Sensoranzahl fort."));

    // Update the configuration to match actual sensor count
    // Update activeMeasurements to match actual detected sensors
    mutableConfig().activeMeasurements = deviceCount;

    // Update the measurement data to match actual sensor count
    m_lastMeasurementData->activeValues = deviceCount;

    // Resize state vectors to match actual sensor count
    m_state.readings.resize(deviceCount);
    m_state.lastValidReadings.resize(deviceCount);
    m_state.consecutiveInvalidCount.resize(deviceCount);

    // Also update the base class samples vector to match the new count
    Sensor::m_state.samples.resize(deviceCount);

    // Update the base class statuses vector to match the new count
    m_statuses.resize(deviceCount, "unknown");

  logger.debug(getName(),
         F("Sensor-Konfiguration aktualisiert: activeMeasurements=") +
           String(config().activeMeasurements) +
           F(", samples.size=") +
           String(Sensor::m_state.samples.size()) +
           F(", statuses.size=") + String(m_statuses.size()));

    if (deviceCount == 0) {
      logger.error(getName(),
                   F("Keine DS18B20-Sensoren gefunden - Initialisierung fehlgeschlagen"));
      return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                                F("Keine DS18B20-Sensoren gefunden"));
    }
  }

  // Set resolution to 10 bits for all found sensors
  m_sensors->setResolution(10);

  logger.info(getName(), F("Initialisiert ") + String(deviceCount) +
                             F(" DS18B20-Sensoren am Pin ") +
                             String(m_oneWireBus));
  m_initialized = true;
  return SensorResult::success();
}

SensorResult DS18B20Sensor::startMeasurement() {
  logDebug(F("Starte DS18B20-Messung"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  if (config().activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
  logger.warning(getName(), F("Begrenze activeMeasurements von ") +
                  String(config().activeMeasurements) +
                  F(" auf ") +
                  String(SensorConfig::MAX_MEASUREMENTS));
    // Note: We can't modify the config directly anymore, but this is just a
    // warning
  }
  m_state.reset(config().activeMeasurements);  // Verwende activeMeasurements
                                               // statt m_sensorCount
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  logger.debug(getName(),
               F("Starte Messung: fordere erste Konversion an"));
  // Start first conversion
  auto requestResult = requestTemperatures();
  logger.debug(getName(),
               F("requestTemperatures() aufgerufen bei ms=") + String(millis()));
  m_state.conversionRequested = true;
  if (!requestResult) {
    logger.error(getName(),
                 F("Anforderung der Temperaturen in startMeasurement fehlgeschlagen"));
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                              F("Anforderung der Temperaturen fehlgeschlagen"));
  }
  return SensorResult::success();
}

SensorResult DS18B20Sensor::continueMeasurement() {
  logDebug(F("Setze DS18B20-Messung fort"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess() || !m_state.readInProgress) {
    return memoryResult;
  }

  // Timeout after 5 seconds
  unsigned long elapsed = millis() - m_state.operationStartTime;
  if (elapsed > 5000) {
  logger.error(getName(),
      F("Messzeitüberschreitung nach ") + String(elapsed) + F(" ms"));
    handleSensorError();
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
            F("Messzeitüberschreitung"));
  }

  bool complete = m_sensors->isConversionComplete();
  logger.debug(getName(), F("isConversionComplete() ergab ") +
                              String(complete ? "true" : "false") +
                              F(" bei ms=") + String(millis()) +
                              F(", vergangen=") + String(elapsed));
  if (!complete) {
    logger.debug(getName(), F("Warte auf Abschluss der Konversion..."));
    return SensorResult::success();  // Still converting
  }

  logger.debug(getName(),
               F("Konversion abgeschlossen nach ") + String(elapsed) + F("ms"));

  // Read all sensors (for base class sample collection)
  // No need to push to m_state.samples, just ensure hardware is ready
  return SensorResult::success();
}

SensorResult DS18B20Sensor::performMeasurementCycle() {
  constexpr unsigned long CONVERSION_TIME = MAX_CONVERSION_TIME;  // ms
  size_t numMeasurements = getNumMeasurements();

  logger.debug(getName(), F("performMeasurementCycle: numMeasurements=") +
                              String(numMeasurements) +
                              F(", config.activeMeasurements=") +
                              String(config().activeMeasurements));

  // Defensive checks
  if (!isInitialized()) {
    logger.error(
        getName(),
        F(": performMeasurementCycle auf nicht initialisiertem Sensor aufgerufen!"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Sensor nicht initialisiert"));
  }
  if (numMeasurements == 0) {
    logger.error(getName(), F(": getNumMeasurements() lieferte 0! Das "
                              "weist auf ein Konfigurationsproblem hin."));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Keine Messungen konfiguriert"));
  }
  if (numMeasurements > SensorConfig::MAX_MEASUREMENTS) {
    logger.error(getName(), F(": getNumMeasurements() returned more than "
                              "MAX_MEASUREMENTS! Clamping."));
    numMeasurements = SensorConfig::MAX_MEASUREMENTS;
  }

  // Reset state at start of cycle
  if (!cycleConversionInProgress && cycleMeasurementIndex == 0) {
    m_state.readings.resize(numMeasurements, 0.0f);
    cycleMeasurementIndex = 0;
    cycleConversionStart = 0;
    cycleConversionInProgress = false;
  }

  // Process each sensor one by one
  while (cycleMeasurementIndex < numMeasurements) {
    if (!cycleConversionInProgress) {
      // Start conversion for this sensor (all sensors on bus convert together)
      if (!requestTemperatures()) {
        logger.error(getName(), F("Anforderung der Temperaturen fehlgeschlagen für Index ") +
                                    String(cycleMeasurementIndex));
        return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                                  F("Anforderung der Temperaturen fehlgeschlagen"));
      }
      cycleConversionStart = millis();
      cycleConversionInProgress = true;
      logDebug(F("Konversion gestartet für Sensor-Index ") +
               String(cycleMeasurementIndex));
      return SensorResult::fail(SensorError::PENDING, "pending");
    }
    // Wait for conversion time
    if (millis() - cycleConversionStart < CONVERSION_TIME) {
      return SensorResult::fail(SensorError::PENDING, "pending");
    }
    // Read value
  float value = m_sensors->getTempCByIndex(cycleMeasurementIndex);
  logDebug(F("Gelesener Wert ") + String(value) + F(" für Sensor-Index ") +
       String(cycleMeasurementIndex));
    m_state.readings[cycleMeasurementIndex] = value;
    cycleMeasurementIndex++;
    cycleConversionInProgress = false;
    // After reading, if more sensors remain, start next conversion on next call
    if (cycleMeasurementIndex < numMeasurements) {
      return SensorResult::fail(SensorError::PENDING, "pending");
    }
  }
  // All sensors read, reset state for next cycle
  cycleMeasurementIndex = 0;
  cycleConversionInProgress = false;
  cycleConversionStart = 0;
  // Copy readings to base class samples for averaging if needed
  if (m_state.readings.size() > 0) {
    Sensor::m_state.samples.clear();
    Sensor::m_state.samples.resize(m_state.readings.size());
    for (size_t i = 0; i < m_state.readings.size(); ++i) {
      Sensor::m_state.samples[i].clear();
      Sensor::m_state.samples[i].push_back(m_state.readings[i]);
    }

  logger.debug(getName(),
           F("performMeasurementCycle abgeschlossen: readings.size=") +
             String(m_state.readings.size()) +
             F(", base_samples.size=") +
             String(Sensor::m_state.samples.size()));
  }
  return SensorResult::success();
}

void DS18B20Sensor::deinitialize() {
  logDebug(F("Deinitialisiere DS18B20-Sensor"));
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.readings);
  Sensor::clearAndShrink(m_state.lastValidReadings);
  m_state.reset(config().activeMeasurements);
}

bool DS18B20Sensor::validateReading(float value) const {
  if (isnan(value)) {
    logger.error(getName(), F("Messwert ist NaN"));
    return false;
  }

  if (value == -127.0f) {
    logger.error(getName(), F("Erhielt getrennten Sensorwert (-127.0°C)"));
    return false;
  }

  if (value == DEVICE_DISCONNECTED_C) {
    logger.error(getName(), F("Gerät scheint getrennt zu sein"));
    return false;
  }

  if (value < -55.0f || value > 125.0f) {
    logger.error(getName(), F("Messwert außerhalb des gültigen Bereichs: ") + String(value) +
                                F("°C (gültig: -55°C bis 125°C)"));
    return false;
  }

  return true;
}

bool DS18B20Sensor::canAccessHardware() const {
  return (millis() - m_state.lastHardwareAccess) >=
         ds18b20Config().minimumDelay;
}

bool DS18B20Sensor::requestTemperatures() {
  if (!m_sensors) {
    logger.error(getName(), F("Invalid sensor object"));
    return false;
  }

  logger.debug(F("Sensors"), getName() + F("Starte Temperatur-Konversion"));
  m_sensors->requestTemperatures();
  m_state.conversionRequested = true;
  m_state.lastHardwareAccess = millis();
  return true;
}

/**
 * @brief Fetch a single sample for a given DS18B20 sensor
 * @param value Reference to store the sample
 * @param index Sensor index
 * @return true if successful, false if hardware error
 */
bool DS18B20Sensor::fetchSample(float& value, size_t index) {
  logDebug(F("Hole DS18B20-Messwert für Index ") + String(index));
  if (!isInitialized()) {
    logger.error(getName(),
                 F(": Versuch, Messwert ohne Initialisierung zu holen"));
    return false;
  }
  DeviceAddress addr;
  String addressStr;
  if (m_sensors->getAddress(addr, index)) {
    for (uint8_t j = 0; j < 8; ++j) {
      if (j > 0) addressStr += ":";
      addressStr += String(addr[j], HEX);
    }
  } else {
    addressStr = F("(unknown)");
  }
  value = m_sensors->getTempCByIndex(index);
  logDebug(F("Gelesener Wert: ") + String(value) + F(" an Index ") +
           String(index) + F(" Adresse: ") + addressStr);
  return !isnan(value);
}

/**
 * @brief Returns the number of measurements for DS18B20Sensor (number of active
 * sensors)
 */
size_t DS18B20Sensor::getNumMeasurements() const {
  return config().activeMeasurements;
}

#endif  // USE_DS18B20
