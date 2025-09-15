#include "sensors/sensor_mhz19.h"

#if USE_MHZ19

MHZ19Sensor::~MHZ19Sensor() {
  // Clean up any remaining samples
  m_state.samples.clear();
  // Reset pin mode to input to ensure clean state
  pinMode(m_mhz19Config.pwmPin, INPUT);
}

MHZ19Sensor::MHZ19Sensor(const MHZ19Config& config,
                         SensorManager* sensorManager)
    : Sensor(config, sensorManager), m_mhz19Config(config) {
  ThresholdDefaults defaults = {MHZ19_YELLOW_LOW, MHZ19_GREEN_LOW,
                                MHZ19_GREEN_HIGH, MHZ19_YELLOW_HIGH};
  mutableConfig().measurements[0].limits.yellowLow = defaults.yellowLow;
  mutableConfig().measurements[0].limits.greenLow = defaults.greenLow;
  mutableConfig().measurements[0].limits.greenHigh = defaults.greenHigh;
  mutableConfig().measurements[0].limits.yellowHigh = defaults.yellowHigh;
  initMeasurement(0, MHZ19_NAME, MHZ19_FIELD_NAME, MHZ19_UNIT,
                  mutableConfig().measurements[0].limits.yellowLow,
                  mutableConfig().measurements[0].limits.greenLow,
                  mutableConfig().measurements[0].limits.greenHigh,
                  mutableConfig().measurements[0].limits.yellowHigh);

  m_state.samples.reserve(REQUIRED_SAMPLES);
}

void MHZ19Sensor::logDebugDetails() const {
  logDebug(F("MHZ19-Konfig: ..."));  // Weitere Details bei Bedarf
}

SensorResult MHZ19Sensor::init() {
  logDebug(F("Initialisiere MHZ19-Sensor"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  // Configure pin with internal pull-down to ensure clean state
  pinMode(m_mhz19Config.pwmPin,
          INPUT);  // ESP8266 doesn't have explicit INPUT_PULLDOWN
  digitalWrite(m_mhz19Config.pwmPin, LOW);  // Use LOW to simulate pull-down

  // Validate config
  if (m_mhz19Config.warmupTime == 0) {
  logger.error(getName(), F("Ungültige Warmup-Zeitkonfiguration (") +
              String(m_mhz19Config.warmupTime) + F("s)"));
    return SensorResult::fail(SensorError::VALIDATION_ERROR,
                              F("Invalid warmup time configuration"));
  }

  // Log initial pin state
  int initialState = digitalRead(m_mhz19Config.pwmPin);
  logger.debug(getName(),
               F("Initialisiert an Pin ") + String(m_mhz19Config.pwmPin) +
                   F(" (Anfangszustand: ") + String(initialState) + F(")"));

  // Monitor pin for a few cycles to check if it's changing
  bool stateChanged = false;
  int lastState = initialState;
  int stableCount = 0;

  // Check for longer period (2 seconds) to catch slower PWM signals
  for (int i = 0; i < 20; i++) {
    delay(100);  // Check every 100ms
    int currentState = digitalRead(m_mhz19Config.pwmPin);
    if (currentState != lastState) {
      stateChanged = true;
      stableCount = 0;
        logger.debug(getName(), F("Pin-Zustand geändert ") + String(lastState) +
                        F(" -> ") + String(currentState));
    } else {
      stableCount++;
    }
    lastState = currentState;
  }

  if (!stateChanged) {
    logger.warning(getName(),
             F("Während der Initialisierung keine Pin-Änderungen erkannt - Überprüfe: ") +
               F("\n1. Stromversorgung (VCC=5V, GND)") +
               F("\n2. PWM-Pin-Verbindung zum GPIO ") +
               String(m_mhz19Config.pwmPin) +
               F("\n3. Sensor-Warmup (benötigt ") +
               String(m_mhz19Config.warmupTime) + F("s)"));
    // Don't fail initialization - give it a chance to warm up
  }

  return SensorResult::success();
}

SensorResult MHZ19Sensor::startMeasurement() {
  logDebug(F("Starting MHZ19 measurement"));
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  logger.debug(getName(), F("Starte Messung"));
  m_state.reset();
  m_state.readInProgress = true;
  m_state.operationStartTime = millis();

  return SensorResult::success();
}

/**
 * @brief Fetch a single sample for the MHZ19 sensor (CO2 concentration)
 * @param value Reference to store the sample
 * @param index Measurement index (should be 0)
 * @return true if successful, false if hardware error
 */
bool MHZ19Sensor::fetchSample(float& value, size_t index) {
  logDebug(F("Lese MHZ19-Probe für Index ") + String(index));
  if (!isInitialized()) {
  logger.error(getName(),
         F(": Versuch, Probe ohne Initialisierung zu lesen"));
    return false;
  }
  value = 0.0f;
  if (!readValue(value)) {
    return false;
  }
  logDebug(F("Gelesener Wert: ") + String(value));
  return !isnan(value);
}

void MHZ19Sensor::deinitialize()
  logDebug(F("Deinitialisiere MHZ19-Sensor"));
  Sensor::deinitialize();
  Sensor::clearAndShrink(m_state.samples);
  m_state = MeasurementState();
}

bool MHZ19Sensor::readValue(float& value) {
  // Wait for potential warmup
  if (millis() < (m_mhz19Config.warmupTime * 1000)) {
  logger.debug(getName(), F("Noch in der Aufwärmphase"));
    return false;
  }

  // Log pin state and attempt to detect any transitions
  int transitions = 0;
  int lastState = digitalRead(m_mhz19Config.pwmPin);
  logger.debug(getName(),
               F("Reading PWM on pin ") + String(m_mhz19Config.pwmPin) +
                   F(" (initial state: ") + String(lastState) + F(")"));

  // Monitor for transitions over a longer period
  for (int i = 0; i < 100; i++) {  // Increased from 20 to 100 samples
    delayMicroseconds(1000);       // 1ms delay
    int currentState = digitalRead(m_mhz19Config.pwmPin);
    if (currentState != lastState) {
      transitions++;
      lastState = currentState;
    }
  }

    logger.debug(getName(), F("Erkannte ") + String(transitions) +
                                F(" Übergänge in 100ms Abtastung"));

  // Measure PWM high and low times with longer timeout if no transitions
  // detected
  unsigned long timeout = transitions == 0 ? PWM_CYCLE * 2 : PWM_CYCLE;
  unsigned long th = pulseIn(m_mhz19Config.pwmPin, HIGH, timeout);
  unsigned long tl = pulseIn(m_mhz19Config.pwmPin, LOW, timeout);

  // Enhanced error logging
  if (th == 0 || tl == 0) {
                logger.error(getName(), F("PWM-Lesen fehlgeschlagen - High-Zeit: ") + String(th) +
                              F("µs, Low-Zeit: ") + String(tl) +
                              F("µs, Zyklus gesamt: ") + String(th + tl) +
                              F("µs"));
    return false;
  }

  // Calculate and validate duty cycle
  float dutyCycle = (float)th / (th + tl) * 100.0f;
              logger.debug(getName(), F("PWM Duty Cycle: ") + String(dutyCycle) +
                                          F("%, High: ") + String(th) + F("µs, Low: ") +
                                          String(tl) + F("µs"));

  // Calculate CO2 concentration
  value = calculatePPM(th, tl);

  if (!validateReading(value)) {
                logger.error(getName(), F("Ungültige Messung: ") + String(value) +
                              F(" ppm (Duty Cycle: ") + String(dutyCycle) +
                              F("%)"));
    return false;
  }

  logger.debug(getName(), F("CO2: ") + String(value) + F(" ppm"));
  return true;
}

bool MHZ19Sensor::validateReading(float value) const {
  return !isnan(value) && value >= MHZ19_MIN && value <= MHZ19_MAX;
}

float MHZ19Sensor::calculatePPM(unsigned long th, unsigned long tl) const {
  // MH-Z19 outputs 2000ppm at 2ms high time and 0ppm at 0.4ms high time
  const float ppmPerMs = 1250.0f;  // (2000ppm / 1.6ms)
  const float minHighTime = 0.4f;  // ms

  float highTimeMs = th / 1000.0f;  // Convert to milliseconds
  return (highTimeMs - minHighTime) * ppmPerMs;
}

#endif  // USE_MHZ19
