#include "sensors/sensor_mhz19.h"

#if USE_MHZ19

// MH-Z19 PWM-Schnittstelle:
// CO2 = (th / (th + tl)) × 2000 − 2 × (th_min / period) × 2000
// Einfache Näherungsformel: CO2_ppm = (th_us / period_us) * 2000
// PWM-Periode = 1004 ms, Bereich: 0..2000 ppm

MHZ19Sensor::~MHZ19Sensor() {
  Sensor::clearAndShrink(m_state.samples);
  pinMode(m_mhz19Config.pwmPin, INPUT);
}

MHZ19Sensor::MHZ19Sensor(const MHZ19Config& config, SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_mhz19Config(config) {
  mutableConfig().measurements[0].limits.yellowLow = MHZ19_YELLOW_LOW;
  mutableConfig().measurements[0].limits.greenLow = MHZ19_GREEN_LOW;
  mutableConfig().measurements[0].limits.greenHigh = MHZ19_GREEN_HIGH;
  mutableConfig().measurements[0].limits.yellowHigh = MHZ19_YELLOW_HIGH;
  initMeasurement(0, MHZ19_NAME, MHZ19_FIELD_NAME, MHZ19_UNIT, MHZ19_YELLOW_LOW, MHZ19_GREEN_LOW,
                  MHZ19_GREEN_HIGH, MHZ19_YELLOW_HIGH);
  m_state.samples.reserve(REQUIRED_SAMPLES);
}

void MHZ19Sensor::logDebugDetails() const {
  logDebug(String(F("MHZ19: pin=")) + String(m_mhz19Config.pwmPin) + String(F(" warmup=")) +
           String(m_mhz19Config.warmupTime) + F("ms"));
}

SensorResult MHZ19Sensor::init() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;

  if (m_mhz19Config.warmupTime == 0) {
    return SensorResult::fail(SensorError::VALIDATION_ERROR, F("Warmup-Zeit = 0"));
  }

  pinMode(m_mhz19Config.pwmPin, INPUT);
  LOG_INFO(getName(), String(F("MHZ19 an Pin ")) + String(m_mhz19Config.pwmPin) +
                          String(F(", Aufwärmzeit ")) + String(m_mhz19Config.warmupTime / 1000) +
                          F("s"));
  m_initialized = true;
  return SensorResult::success();
}

SensorResult MHZ19Sensor::startMeasurement() {
  auto memResult = validateMemoryState();
  if (!memResult.isSuccess())
    return memResult;
  m_state.reset();
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();
  return SensorResult::success();
}

SensorResult MHZ19Sensor::continueMeasurement() {
  // PWM-Lesen ist in fetchSample – hier nichts zu tun
  return SensorResult::success();
}

bool MHZ19Sensor::fetchSample(float& value, size_t /*index*/) {
  if (!isInitialized())
    return false;
  return readValue(value);
}

void MHZ19Sensor::deinitialize() {
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.samples);
  m_state = MeasurementState();
}

bool MHZ19Sensor::readValue(float& value) {
  // Aufwärmphase prüfen (warmupTime ist in Millisekunden)
  if (millis() < m_mhz19Config.warmupTime) {
    logDebug(F("Noch in Aufwärmphase"));
    return false;
  }

  // PWM-Messung: pulseIn() gibt Microsekunden zurück.
  // Timeout = 1,5 × PWM-Periode in Mikrosekunden.
  // pulseIn() blockiert maximal diese Zeit – bei 1004ms Periode ca. 1,5s.
  // Da MHZ19 sehr selten gemessen wird (z.B. alle 5min), ist das akzeptabel.
  constexpr unsigned long TIMEOUT_US = PWM_CYCLE * 1500UL; // 1.5 Perioden in µs

  unsigned long th = pulseIn(m_mhz19Config.pwmPin, HIGH, TIMEOUT_US);
  unsigned long tl = pulseIn(m_mhz19Config.pwmPin, LOW, TIMEOUT_US);

  // If low pulse timed out, try one phase-resync attempt.
  // Some MH-Z19 modules occasionally lose edge alignment for one cycle.
  if (th > 0 && tl == 0) {
    unsigned long tlRetry = pulseIn(m_mhz19Config.pwmPin, LOW, TIMEOUT_US);
    unsigned long thRetry = pulseIn(m_mhz19Config.pwmPin, HIGH, TIMEOUT_US);
    if (thRetry > 0 && tlRetry > 0) {
      th = thRetry;
      tl = tlRetry;
      LOG_DEBUG(getName(), F("PWM-Resync erfolgreich"));
    }
  }

  // Always log PWM values for debugging
  LOG_DEBUG(getName(),
            String(F("PWM: th=")) + String(th) + String(F("µs tl=")) + String(tl) + F("µs"));

  if (th == 0 || tl == 0) {
    LOG_WARN(getName(), String(F("PWM-Lesung fehlgeschlagen (th=")) + String(th) +
                            String(F(" tl=")) + String(tl) + F(")"));
    return false;
  }

  value = calculatePPM(th, tl);

  if (!validateReading(value)) {
    LOG_WARN(getName(), String(F("Ungültiger CO2-Wert: ")) + String(value) +
                            String(F(" ppm (th=")) + String(th) + String(F("µs tl=")) + String(tl) +
                            F("µs)"));
    return false;
  }

  LOG_DEBUG(getName(), String(F("CO2: ")) + String(value) + F(" ppm"));
  return true;
}

bool MHZ19Sensor::validateReading(float value) const {
  return !isnan(value) && isfinite(value) && value > 0.0f && value <= MHZ19_MAX;
}

void MHZ19Sensor::handleSensorError() {
  m_errorState.errorCount++;

  if (m_errorState.errorCount >= MAX_RETRIES) {
    // Keep MH-Z19 initialized. PWM glitches can happen transiently and the
    // sensor often recovers without full reinitialization.
    LOG_ERROR(getName(), F(": Zu viele MHZ19-Lesefehler, Sensor bleibt "
                           "aktiv und versucht erneut"));
    m_errorState.errorCount = 0;
    m_errorState.invalidCount = 0;
    m_errorState.inRetryDelay = false;
  }
}

float MHZ19Sensor::calculatePPM(unsigned long th_us, unsigned long tl_us) const {
  // MH-Z19 PWM-Formel laut Datenblatt:
  // Cppm = RANGE × (th - 2ms) / (th + tl - 4ms)
  // th und tl sind in Microsekunden
  // RANGE = 2000, 5000, oder 10000 je nach Sensorvariante
  const float th_ms = th_us / 1000.0f;
  const float tl_ms = tl_us / 1000.0f;
  const float denom = th_ms + tl_ms - 4.0f;
  if (denom <= 0.0f)
    return 0.0f;
  return m_mhz19Config.range * (th_ms - 2.0f) / denom;
}

#endif // USE_MHZ19
