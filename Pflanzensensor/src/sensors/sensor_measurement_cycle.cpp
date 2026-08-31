#include "sensor_measurement_cycle.h"

#if USE_DS18B20
// #include "sensors/sensor_ds18b20.h" // TODO: Add when driver ready // Add include for DS18B20 sensor
#endif
SensorMeasurementCycleManager::SensorMeasurementCycleManager(Sensor* sensor)
    : m_sensor(sensor),
      m_state(),
      m_lastState(MeasurementState::WAITING_FOR_DUE),
      m_lastSlotAttemptTime(0) {
  if (m_sensor) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"),
                String(F("Initialisiere Zyklus-Manager für Sensor: ")) + m_sensor->getName());
    }

    // Check warmup requirements
    m_state.needsWarmup = m_sensor->requiresWarmup(m_state.warmupTimeNeeded);
    if (m_state.needsWarmup) {
      m_state.warmupStartTime = millis(); // Starte Aufwärmphase sofort
      if (ConfigMgr.isDebugMeasurementCycle()) {
        LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() +
                                             String(F(": Starte Aufwärmphase von ")) +
                                             String(m_state.warmupTimeNeeded / 1000UL) + F("s"));
      }
    }

    // Store the measurement interval
    m_state.measurementInterval = m_sensor->getMeasurementInterval();

    // Record the start time of the first cycle
    m_cycleStartTime = millis();

    // Schedule first measurement based on cycle start time
    m_state.scheduleNextMeasurement(m_cycleStartTime, 0); // Start immediately
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), F("Erste Messung für sofortige Ausführung geplant"));
    }
  } else {
    LOG_ERROR(F("MeasurementCycle"), F("Created with null sensor!"));
  }
}

SensorMeasurementCycleManager::MeasurementDoneCallback
    SensorMeasurementCycleManager::s_measurementDone = nullptr;

bool SensorMeasurementCycleManager::updateMeasurementCycle() {
  if (!m_sensor) {
    return false;
  }

  // Update measurement interval in case it changed
  unsigned long currentInterval = m_sensor->getMeasurementInterval();
  if (currentInterval != m_state.measurementInterval) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"),
                m_sensor->getName() + String(F(": Messintervall aktualisiert von ")) +
                    String(m_state.measurementInterval) + String(F("ms auf ")) +
                    String(currentInterval) + F("ms"));
    }
    m_state.measurementInterval = currentInterval;
  }

  // Sicherheitsnetz 1: Zeitschranke des aktuellen Zustands
  const unsigned long stateLimit = stateTimeoutFor(m_state.state);
  if (stateLimit > 0 && millis() - m_state.stateStartTime >= stateLimit) {
    handleStateError(String(F("Zeitüberschreitung im Zustand ")) +
                     MeasurementStateInfo::stateToString(m_state.state) + String(F(" nach ")) +
                     String(stateLimit) + F(" ms"));
    return false;
  }

  // Sicherheitsnetz 2: Wir glauben zu messen, halten den Slot aber nicht mehr.
  // Das passiert, wenn der Limiter ihn wegen Zeitüberschreitung zwangsweise
  // freigegeben hat - ohne diesen Abbruch würde parallel zu einem anderen
  // Sensor weitergemessen, auf gemeinsamer Hardware also mit falschen Werten.
  if (holdsSlotInState(m_state.state) &&
      !SensorManagerLimiter::getInstance().hasSlot(m_sensor->getId())) {
    LOG_WARN(F("MeasurementCycle"),
             m_sensor->getName() + F(": Messslot verloren, Zyklus wird abgebrochen"));
    abortCycle();
    return false;
  }

  switch (m_state.state) {
  case MeasurementState::WAITING_FOR_DUE:
    return handleWaitingForDue();
  case MeasurementState::WAITING_FOR_SLOT:
    handleWaitingForSlot();
    break;
  case MeasurementState::WAITING_FOR_DELAY:
    handleWaitingForDelay();
    break;
  case MeasurementState::INITIALIZING:
    handleInitializing();
    break;
  case MeasurementState::WARMUP:
    handleWarmup();
    break;
  case MeasurementState::MEASURING:
    handleMeasuring();
    break;
  case MeasurementState::PROCESSING:
    handleProcessing();
    break;
  case MeasurementState::DEINITIALIZING:
    handleDeinitializing();
    break;
  case MeasurementState::ERROR:
    handleError();
    break;
  default:
    handleUnknownState();
    break;
  }

  return false;
}

void SensorMeasurementCycleManager::tick() {
  if (!m_sensor) {
    return;
  }

  MeasurementState currentState = m_state.state;

  const bool stateChanged = (currentState != m_lastLoggedState);
  m_lastLoggedState = currentState;
  if (stateChanged && ConfigMgr.isDebugMeasurementCycle()) {
    LOG_DEBUG(F("MeasurementCycle"), m_sensor->getId() + String(F(" Zustand: ")) +
                                         String(static_cast<int>(currentState)) + F(" (geändert)"));
  }

  const bool shouldProcess = (currentState != MeasurementState::WAITING_FOR_DUE) || isDue();
  if (!shouldProcess) {
    return;
  }

  const bool cycleResult = updateMeasurementCycle();
  const bool resultChanged = (cycleResult != m_lastUpdateResult);
  m_lastUpdateResult = cycleResult;
  if (resultChanged && ConfigMgr.isDebugMeasurementCycle()) {
    LOG_DEBUG(F("MeasurementCycle"),
              m_sensor->getId() + String(F(" Zyklus: ")) +
                  (cycleResult ? String(F("Abgeschlossen")) : String(F("In Bearbeitung"))) +
                  F(" (geändert)"));
  }
}

bool SensorMeasurementCycleManager::holdsSlotInState(MeasurementState state) {
  switch (state) {
  case MeasurementState::INITIALIZING:
  case MeasurementState::WAITING_FOR_DELAY:
  case MeasurementState::WARMUP:
  case MeasurementState::MEASURING:
  case MeasurementState::PROCESSING:
  case MeasurementState::DEINITIALIZING:
    return true;
  default:
    return false;
  }
}

unsigned long SensorMeasurementCycleManager::stateTimeoutFor(MeasurementState state) const {
  switch (state) {
  case MeasurementState::INITIALIZING:
    return STATE_TIMEOUT_INIT;
  case MeasurementState::WAITING_FOR_DELAY:
    return STATE_TIMEOUT_DELAY;
  case MeasurementState::WARMUP:
    // Die Aufwärmzeit ist sensorabhängig, deshalb relativ dazu.
    return m_state.warmupTimeNeeded + STATE_TIMEOUT_WARMUP_MARGIN;
  case MeasurementState::MEASURING:
    return MEASURE_TIMEOUT;
  case MeasurementState::PROCESSING:
    return STATE_TIMEOUT_PROCESSING;
  case MeasurementState::DEINITIALIZING:
    return STATE_TIMEOUT_DEINIT;
  default:
    // WAITING_FOR_DUE (einmalige Aufwärmphase), WAITING_FOR_SLOT (eigene
    // Behandlung über SLOT_TIMEOUT) und ERROR (eigene Wiederholungslogik)
    return 0;
  }
}

void SensorMeasurementCycleManager::abortCycle() {
  if (!m_sensor) {
    return;
  }

  auto& limiter = SensorManagerLimiter::getInstance();
  const bool heldSlot = limiter.hasSlot(m_sensor->getId());
  const bool wasBusy = (m_state.state != MeasurementState::WAITING_FOR_DUE);

  if (wasBusy) {
    LOG_INFO(F("MeasurementCycle"), m_sensor->getName() + F(": Laufende Messung wird abgebrochen"));
    // Der Sensor wurde für diesen Zyklus eingeschaltet und muss wieder
    // abgeschaltet werden, sonst bleibt z.B. der Multiplexer auf einem Kanal
    // stehen und der nächste Zyklus initialisiert nie neu.
    if (m_sensor->isInitialized() && m_sensor->shouldDeinitializeAfterMeasurement()) {
      m_sensor->deinitialize();
    }
  }

  if (heldSlot) {
    limiter.releaseSlot(m_sensor->getId());
  }

  m_forced = false;
  m_currentResults.clear();
  m_state.errorCount = 0;
  m_state.warmupStartTime = 0;
  m_state.warmupDoneThisCycle = false;
  m_state.minimumDelayEndTime = 0;
  m_state.measurementStarted = false;
  m_slotRequestStartTime = 0;
  m_lastSlotAttemptTime = 0;
  m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
}

void SensorMeasurementCycleManager::forceImmediateMeasurement() {
  if (!m_sensor) {
    return;
  }

  // Eigenen Zyklus abbrechen — gibt insbesondere einen selbst gehaltenen Slot
  // frei, den der Sensor sich sonst nicht mehr selbst zuteilen könnte.
  abortCycle();

  m_forced = true;
  m_state.nextDueTime = millis();
  m_cycleStartTime = millis();
}

void SensorMeasurementCycleManager::reset() {
  m_state.reset();
  m_lastState = MeasurementState::WAITING_FOR_DUE;
  m_currentResults.clear();
  m_cycleStartTime = 0;
  m_lastDebugTime = 0;
  m_lastSlotAttemptTime = 0; // Reset slot attempt time
  m_forced = false;
  m_retryLevel = 0;
}

MeasurementState SensorMeasurementCycleManager::getCurrentState() const { return m_state.state; }

const String& SensorMeasurementCycleManager::getLastError() const { return m_state.lastError; }
