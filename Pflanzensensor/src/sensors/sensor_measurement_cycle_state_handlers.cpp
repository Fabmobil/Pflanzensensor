#include "sensor_measurement_cycle.h"

bool SensorMeasurementCycleManager::handleWaitingForDue() {
  unsigned long now = millis();

  // Check if sensor is still in warmup
  if (m_state.needsWarmup) {
    unsigned long warmupElapsed = now - m_state.warmupStartTime;
    if (warmupElapsed < m_state.warmupTimeNeeded) {
      // Still in warmup period
      if (ConfigMgr.isDebugMeasurementCycle() &&
          (now - m_lastDebugTime >= DEBUG_INTERVAL)) {
        unsigned long remaining =
            (m_state.warmupTimeNeeded - warmupElapsed) / 1000UL;
        logger.debug(F("MeasurementCycle"),
                     m_sensor->getName() + F(": Warmup in progress, ") +
                         String(remaining) + F("s remaining"));
        m_lastDebugTime = now;
      }
      return false;
    }
    // Warmup complete
    m_state.needsWarmup = false;
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Warmup complete"));
    }
  }

  if (!m_state.isDue()) {
    // Not time yet, check if we should log debug info
    if (ConfigMgr.isDebugMeasurementCycle() &&
        (now - m_lastDebugTime >= DEBUG_INTERVAL)) {
      logger.debug(
          F("MeasurementCycle"),
          m_sensor->getName() + F(": Next measurement due in ") +
              String((m_state.nextDueTime > now) ? (m_state.nextDueTime - now)
                                                 : 0) +
              F("ms"));
      m_lastDebugTime = now;
    }
    return false;
  }

  // Record the start time of this measurement cycle
  m_cycleStartTime = now;

  if (ConfigMgr.isDebugMeasurementCycle()) {
    logger.debug(F("MeasurementCycle"),
                 m_sensor->getName() +
                     F(": Measurement interval elapsed, requesting slot"));
  }

  m_state.setState(MeasurementState::WAITING_FOR_SLOT, m_sensor->getName());
  return true;
}

void SensorMeasurementCycleManager::handleWaitingForSlot() {
  static constexpr unsigned long SLOT_RETRY_DELAY = 50;  // Reduced from 100ms
  unsigned long now = millis();

  // Only log first attempt and state changes
  static bool firstAttempt = true;
  static bool lastSlotResult = false;

  // Check for slot timeout
  if (m_slotRequestStartTime > 0 &&
      now - m_slotRequestStartTime >= SLOT_TIMEOUT) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.warning(F("MeasurementCycle"),
                     m_sensor->getName() +
                         F(": Slot request timed out after ") +
                         String(SLOT_TIMEOUT) + F("ms"));
    }
    // Reset slot request time and go back to waiting for due
    m_slotRequestStartTime = 0;
    m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
    return;
  }

  unsigned long timeSinceLastTry = now - m_lastSlotAttemptTime;
  if (timeSinceLastTry < SLOT_RETRY_DELAY) {
    return;
  }

  // Initialize slot request start time on first attempt
  if (m_slotRequestStartTime == 0) {
    m_slotRequestStartTime = now;
  }

  m_lastSlotAttemptTime = now;
  bool slotAcquired =
      SensorManagerLimiter::getInstance().acquireSlot(m_sensor->getId());

  // Log only on first attempt or when result changes
  if (firstAttempt || slotAcquired != lastSlotResult) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Slot acquisition ") +
                       (slotAcquired ? F("succeeded") : F("failed")) +
                       F(" at ") + String(now - m_slotRequestStartTime) +
                       F("ms"));
    }
    firstAttempt = false;
    lastSlotResult = slotAcquired;
  }

  if (slotAcquired) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(
          F("MeasurementCycle"),
          m_sensor->getName() + F(": Starting initialization sequence"));
    }
    m_state.setState(MeasurementState::INITIALIZING, m_sensor->getName());
    firstAttempt = true;         // Reset for next cycle
    m_slotRequestStartTime = 0;  // Reset slot request time
  }
}

void SensorMeasurementCycleManager::handleWaitingForDelay() {
  if (!m_state.isMinimumDelayElapsed()) {
    return;
  }

  if (m_state.needsWarmup && m_state.warmupStartTime == 0) {
    m_state.setState(MeasurementState::WARMUP, m_sensor->getName());
  } else {
    m_state.setState(MeasurementState::MEASURING, m_sensor->getName());
  }
}

void SensorMeasurementCycleManager::handleWarmup() {
  if (m_state.warmupStartTime == 0) {
    m_state.warmupStartTime = millis();
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Starting warmup period"));
    }
  }

  if (millis() - m_state.warmupStartTime >= m_state.warmupTimeNeeded) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      logger.debug(F("MeasurementCycle"),
                   m_sensor->getName() + F(": Warmup complete"));
    }
    m_state.warmupStartTime = 0;
    m_state.setMinimumDelay(WARMUP_DELAY);
    m_state.setState(MeasurementState::WAITING_FOR_DELAY, m_sensor->getName());
  }
}

void SensorMeasurementCycleManager::handleMeasuring() {
  // [CHANGED: Use new performMeasurementCycle() method on sensor]
  auto result = m_sensor->performMeasurementCycle();
  if (result.error().has_value() &&
      result.error().value() == SensorError::PENDING) {
    // Still in progress, wait for next call
    return;
  }
  if (!result.isSuccess()) {
    handleStateError(F("Measurement failed in performMeasurementCycle"));
    return;
  }
  m_currentResults = m_sensor->getAveragedResults();  // Use DRY base method
  logger.debug(F("MeasurementCycle"),
               m_sensor->getName() + F(": Moving to processing state"));
  m_state.setState(MeasurementState::PROCESSING, m_sensor->getName());
}
