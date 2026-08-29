#include "sensors/sensor_hx711.h"

#if USE_HX711

HX711Sensor::~HX711Sensor() {
  Sensor::clearAndShrink(m_state.samples);
  if (m_scale)
    m_scale->power_down();
}

HX711Sensor::HX711Sensor(const HX711Config& config, SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_scale(std::make_unique<HX711>()) {
  mutableConfig().measurements[0].limits.yellowLow = HX711_YELLOW_LOW;
  mutableConfig().measurements[0].limits.greenLow = HX711_GREEN_LOW;
  mutableConfig().measurements[0].limits.greenHigh = HX711_GREEN_HIGH;
  mutableConfig().measurements[0].limits.yellowHigh = HX711_YELLOW_HIGH;
  initMeasurement(0, HX711_NAME, HX711_FIELD_NAME, HX711_UNIT, HX711_YELLOW_LOW, HX711_GREEN_LOW,
                  HX711_GREEN_HIGH, HX711_YELLOW_HIGH);
  m_state.samples.reserve(REQUIRED_SAMPLES);
}

void HX711Sensor::logDebugDetails() const {
  logDebug(String(F("HX711: DOUT=")) + String(hx711Config().doutPin) + String(F(" SCK=")) +
           String(hx711Config().sckPin));
}

SensorResult HX711Sensor::init() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;

  m_scale->begin(hx711Config().doutPin, hx711Config().sckPin);

  if (!m_scale->is_ready()) {
    LOG_WARN(getName(), F("HX711 nicht bereit – Hardware evtl. nicht angeschlossen"));
    // Kein Fehler zurückgeben – graceful degradation via Fehlercount
  }

  // Kalibrierungswert aus Config; tare() blockiert kurz, aber nur beim Start
  m_scale->set_scale(HX711_SCALE);
  m_scale->tare();

  LOG_INFO(getName(), String(F("HX711 initialisiert (DOUT:")) + String(hx711Config().doutPin) +
                          String(F(" SCK:")) + String(hx711Config().sckPin) + F(")"));
  return SensorResult::success();
}

SensorResult HX711Sensor::startMeasurement() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;
  m_state.reset();
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  return SensorResult::success();
}

SensorResult HX711Sensor::continueMeasurement() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess() || !m_state.readInProgress)
    return memResult;

  if (millis() - m_state.operationStartTime > 5000UL) {
    LOG_ERROR(getName(), F("Messung Timeout"));
    return SensorResult::fail(SensorError::MEASUREMENT_ERROR, F("Timeout"));
  }

  if (!canAccessHardware())
    return SensorResult::success();

  float value;
  if (!readValue(value)) {
    auto r = handleInvalidReading(value);
    return r.isSuccess() ? SensorResult::success() : r;
  }

  resetInvalidCount();
  if (m_state.sampleCount < REQUIRED_SAMPLES) {
    m_state.samples.push_back(value);
    m_state.sampleCount++;
  }
  m_state.lastHardwareAccess = millis();

  if (m_state.sampleCount >= REQUIRED_SAMPLES) {
    processResults();
    return SensorResult::fail(SensorError::SUCCESS, F("Messung abgeschlossen"));
  }
  return SensorResult::success();
}

void HX711Sensor::deinitialize() {
  if (m_scale)
    m_scale->power_down();
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.samples);
  m_state = MeasurementState();
}

bool HX711Sensor::readValue(float& value) {
  if (!m_scale || !m_scale->is_ready())
    return false;
  value = m_scale->get_units(1);
  return validateReading(value);
}

void HX711Sensor::processResults() {
  if (m_state.samples.empty())
    return;
  float sum = 0.0f;
  for (float s : m_state.samples)
    sum += s;
  float avg = sum / static_cast<float>(m_state.samples.size());
  logDebug(String(F("HX711 ø=")) + String(avg) + String(F("g aus ")) +
           String(m_state.samples.size()) + F(" Samples"));
  m_state.samples.clear();
  m_state.samples.push_back(avg); // Für fetchSample bereitstellen
  m_state.sampleCount = 0;
}

bool HX711Sensor::validateReading(float value) const { return !isnan(value); }

bool HX711Sensor::canAccessHardware() const {
  return (millis() - m_state.lastHardwareAccess) >= hx711Config().minimumDelay;
}

bool HX711Sensor::fetchSample(float& value, size_t /*index*/) {
  if (!isInitialized())
    return false;
  return readValue(value);
}

#endif // USE_HX711
