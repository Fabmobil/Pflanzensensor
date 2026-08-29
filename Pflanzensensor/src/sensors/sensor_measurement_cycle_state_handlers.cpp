/**
 * @file sensor_measurement_cycle_state_handlers.cpp
 * @brief Zustandshandler für den Messzyklus
 * @details Implementiert die Zustandsübergänge der Messzyklus-Zustandsmaschine:
 *          WAITING_FOR_DUE → WAITING_FOR_SLOT → INITIALIZING → WAITING_FOR_DELAY
 *          → [WARMUP → WAITING_FOR_DELAY] → MEASURING → PROCESSING
 */

#include "sensor_measurement_cycle.h"

/**
 * @brief Behandelt den WAITING_FOR_DUE-Zustand
 * @return false (Zyklus nie als abgeschlossen melden — der Aufrufer prüft isDue())
 * @details Wartet auf den nächsten Messzeitpunkt. Prüft auch initiale
 *          Aufwärmphase bei Sensoren, die nach dem Einschalten Zeit brauchen.
 */
bool SensorMeasurementCycleManager::handleWaitingForDue() {
  unsigned long now = millis();

  // Initiale Aufwärmphase prüfen (nur beim ersten Start, nicht pro Messzyklus)
  if (m_state.needsWarmup) {
    unsigned long warmupElapsed = now - m_state.warmupStartTime;
    if (warmupElapsed < m_state.warmupTimeNeeded) {
      // Aufwärmphase läuft noch. Auch eine manuell ausgelöste Messung muss
      // hier warten — der Sensor liefert vor Ablauf der Aufwärmzeit schlicht
      // keine gültigen Werte. Damit das nicht wie ein Hänger aussieht, wird
      // es in dem Fall unabhängig vom Debug-Schalter protokolliert.
      if (m_forced && (now - m_lastDebugTime >= DEBUG_INTERVAL)) {
        LOG_INFO(
            F("MeasurementCycle"),
            m_sensor->getName() + String(F(": Messung angefordert, Sensor wärmt noch auf - ")) +
                String((m_state.warmupTimeNeeded - warmupElapsed) / 1000UL) + F(" s verbleibend"));
        m_lastDebugTime = now;
      }
      if (ConfigMgr.isDebugMeasurementCycle() && (now - m_lastDebugTime >= DEBUG_INTERVAL)) {
        unsigned long remaining = (m_state.warmupTimeNeeded - warmupElapsed) / 1000UL;
        LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + String(F(": Aufwärmphase läuft, ")) +
                                             String(remaining) + F(" s verbleibend"));
        m_lastDebugTime = now;
      }
      return false;
    }
    // Aufwärmphase abgeschlossen
    m_state.needsWarmup = false;
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Aufwärmen abgeschlossen"));
    }
  }

  // Messzeitpunkt noch nicht erreicht
  if (!m_state.isDue()) {
    return false;
  }

  // Messzyklusbeginn protokollieren
  m_cycleStartTime = now;

  if (ConfigMgr.isDebugMeasurementCycle()) {
    LOG_DEBUG(F("MeasurementCycle"),
              m_sensor->getName() + F(": Messintervall abgelaufen, fordere Slot an"));
  }

  m_state.warmupDoneThisCycle = false;
  m_state.setState(MeasurementState::WAITING_FOR_SLOT, m_sensor->getName());
  return false; // Zyklus ist NICHT abgeschlossen — er beginnt gerade erst
}

/**
 * @brief Behandelt den WAITING_FOR_SLOT-Zustand
 * @details Versucht einen Messslot zu reservieren. Bei Timeout zurück zu WAITING_FOR_DUE.
 *          Verwendet Instanzvariablen statt statischer Variablen, damit mehrere
 *          Sensorinstanzen sich nicht gegenseitig beeinflussen.
 */
void SensorMeasurementCycleManager::handleWaitingForSlot() {
  unsigned long now = millis();

  // Timeout-Prüfung: Slot konnte nicht rechtzeitig reserviert werden
  if (m_slotRequestStartTime > 0 && now - m_slotRequestStartTime >= SLOT_TIMEOUT) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_WARN(F("MeasurementCycle"), m_sensor->getName() +
                                          String(F(": Slot-Anfrage Zeitüberschreitung nach ")) +
                                          String(SLOT_TIMEOUT) + F(" ms"));
    }
    m_slotRequestStartTime = 0;
    // Nächste Messung sofort erneut versuchen
    m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
    return;
  }

  // Wartezeit zwischen Versuchen einhalten
  if (now - m_lastSlotAttemptTime < SLOT_RETRY_DELAY) {
    return;
  }

  // Ersten Versuch initialisieren
  if (m_slotRequestStartTime == 0) {
    m_slotRequestStartTime = now;
  }

  m_lastSlotAttemptTime = now;
  bool slotAcquired = SensorManagerLimiter::getInstance().acquireSlot(m_sensor->getId());

  if (slotAcquired) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + String(F(": Slot reserviert nach ")) +
                                           String(now - m_slotRequestStartTime) + F(" ms"));
    }
    m_slotRequestStartTime = 0;
    m_state.setState(MeasurementState::INITIALIZING, m_sensor->getName());
  }
}

/**
 * @brief Behandelt den WAITING_FOR_DELAY-Zustand
 * @details Wartet minimale Verzögerung ab (nach Init oder Warmup).
 *          Entscheidet basierend auf Sensor-Anforderungen, ob Warmup oder
 *          Messung als nächstes kommt.
 */
void SensorMeasurementCycleManager::handleWaitingForDelay() {
  if (!m_state.isMinimumDelayElapsed()) {
    return;
  }

  // Nach Initialisierung: Prüfen ob Sensor pro Messzyklus ein Warmup braucht
  if (m_sensor->isMeasurementWarmupSensor() && !m_state.warmupDoneThisCycle) {
    m_state.setState(MeasurementState::WARMUP, m_sensor->getName());
  } else {
    m_state.setState(MeasurementState::MEASURING, m_sensor->getName());
  }
}

/**
 * @brief Behandelt den WARMUP-Zustand
 * @details Führt die sensorspezifische Aufwärmphase durch (z.B. DS18B20 braucht
 *          750ms nach Initialisierung). Nach Abschluss mit kurzer Verzögerung
 *          zum Messzustand weiter.
 */
void SensorMeasurementCycleManager::handleWarmup() {
  if (m_state.warmupStartTime == 0) {
    m_state.warmupStartTime = millis();
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Starte Aufwärmphase"));
    }
  }

  if (millis() - m_state.warmupStartTime >= m_state.warmupTimeNeeded) {
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Aufwärmen abgeschlossen"));
    }
    m_state.warmupStartTime = 0;
    m_state.warmupDoneThisCycle = true;
    m_state.setMinimumDelay(WARMUP_DELAY);
    m_state.setState(MeasurementState::WAITING_FOR_DELAY, m_sensor->getName());
  }
}

/**
 * @brief Behandelt den MEASURING-Zustand
 * @details Führt den Messzyklus des Sensors durch. Unterstützt asynchrone
 *          Messungen (PENDING-Status) für langsame Sensoren.
 *          Bei Erfolg werden die gemittelten Ergebnisse gespeichert und
 *          der Zeitstempel der letzten Messung aktualisiert.
 */
void SensorMeasurementCycleManager::handleMeasuring() {
  auto result = m_sensor->performMeasurementCycle();

  // Asynchrone Messung noch nicht fertig
  if (result.error().has_value() && result.error().value() == SensorError::PENDING) {
    return;
  }

  // Messung fehlgeschlagen
  if (!result.isSuccess()) {
    handleStateError(String(F("Messung fehlgeschlagen: ")) + result.getMessage());
    return;
  }

  // Ergebnisse sichern
  m_currentResults = m_sensor->getAveragedResults();

  // Erfolg hebt die Fehlerhistorie auf.
  //
  // MEASUREMENT_ERROR_COUNT ist als "Anzahl aufeinanderfolgender
  // Fehlmessungen" dokumentiert, errorCount wurde bei einer erfolgreichen
  // Messung aber nirgends zurückgesetzt - nur nach geglückter
  // Reinitialisierung. Ein Sensor, der gelegentlich eine Messung verpatzt
  // (beim DHT völlig normal), summierte die Fehler über Stunden auf und lief
  // in die Fehlerbehandlung, obwohl er funktionierte.
  m_state.errorCount = 0;
  m_state.fatalError = false;
  m_retryLevel = 0;
  if (m_sensor->config().hasPersistentError) {
    LOG_INFO(F("MeasurementCycle"), m_sensor->getName() + F(": Liefert wieder Messwerte"));
    m_sensor->mutableConfig().hasPersistentError = false;
  }

  // Zeitstempel der erfolgreichen Messung aktualisieren (nur hier, nicht in Processing)
  m_sensor->updateLastMeasurementTime();

  LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Wechsel in Verarbeitungszustand"));
  m_state.setState(MeasurementState::PROCESSING, m_sensor->getName());
}
