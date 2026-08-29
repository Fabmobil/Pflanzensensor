/**
 * @file sensor_measurement_cycle_error_handling.cpp
 * @brief Fehlerbehandlung für den Messzyklus
 * @details Verwaltet Fehlerzustände, Wiederholungslogik und Sensor-Deaktivierung.
 *
 * Fehlerfluss:
 *   1. handleStateError() wird aufgerufen → merkt sich vorherigen Zustand,
 *      gibt ggf. Slot frei, setzt Zustand auf ERROR
 *   2. Nächster updateMeasurementCycle()-Aufruf → handleError() behandelt
 *      die Wiederholungslogik und ggf. Reinitialisierung
 *
 * WICHTIG: recordError() inkrementiert errorCount bereits.
 *          handleError() darf errorCount NICHT erneut inkrementieren.
 */

#include "sensor_measurement_cycle.h"

/**
 * @brief Behandelt den ERROR-Zustand
 * @details Entscheidet basierend auf Fehleranzahl über Wiederholung oder
 *          Sensor-Deaktivierung. Gibt den Slot NICHT erneut frei — das
 *          hat handleStateError() bereits erledigt.
 *
 * Fehlerbehandlung:
 *   - Unter MEASUREMENT_ERROR_COUNT: Deinitialisierung + Wartezeit + erneuter Versuch
 *   - Bei MEASUREMENT_ERROR_COUNT: Reinitialisierungsversuch
 *   - Falls Reinitialisierung scheitert: Sensor deaktivieren (oder Neustart bei DS18B20)
 */
void SensorMeasurementCycleManager::handleError() {
  // Prüfen ob maximale Fehleranzahl erreicht
  if (m_state.errorCount >= MEASUREMENT_ERROR_COUNT) {
    LOG_WARN(F("MeasurementCycle"),
             m_sensor->getName() + String(F(": Maximale Fehleranzahl erreicht (")) +
                 String(m_state.errorCount) + F("), versuche Reinitialisierung"));

    // Sensor deinitialisieren falls nötig
    if (m_sensor->isInitialized()) {
      m_sensor->deinitialize();
    }

    // Reinitialisierungsversuch
    if (m_sensor->init().isSuccess()) {
      // Erfolg — Fehler zurücksetzen und weitermachen
      if (m_sensor->config().hasPersistentError) {
        LOG_INFO(F("MeasurementCycle"),
                 m_sensor->getName() + F(": Erfolgreich reinitialisiert nach persistentem Fehler"));
        m_sensor->mutableConfig().hasPersistentError = false;
      }
      m_state.errorCount = 0;
      m_state.fatalError = false;
      m_state.scheduleNextMeasurement(millis(), m_state.measurementInterval);
      m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
      return;
    }

    // Reinitialisierung fehlgeschlagen
    LOG_ERROR(F("MeasurementCycle"), m_sensor->getName() + F(": Reinitialisierung fehlgeschlagen"));
    m_sensor->mutableConfig().hasPersistentError = true;

    // DS18B20: Neustart weil Hardware-Reset nötig
    if (m_sensor->getSharedHardwareInfo().type == SensorType::DS18B20) {
      LOG_ERROR(F("MeasurementCycle"),
                m_sensor->getName() + F(": DS18B20-Fehler, löse Neustart aus"));
      delay(1000);
      ESP.restart();
      return;
    }

    // Andere Sensoren: aktiviert lassen, nur seltener versuchen
    scheduleRetryWithBackoff();
    return;
  }

  // Unter maximaler Fehleranzahl: Deinitialisieren und erneut versuchen
  if (m_sensor->isInitialized()) {
    m_sensor->deinitialize();
    m_state.needsInitialization = true;
  }

  // Wartezeit vor erneutem Versuch
  if (millis() - m_state.lastErrorTime >= ERROR_RETRY_DELAY) {
    m_state.scheduleNextMeasurement(millis(), m_state.measurementInterval);
    m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
  }
}

/**
 * @brief Behandelt unbekannte Zustände
 * @details Sicherheitsnetz — sollte nie auftreten. Löst Fehlerbehandlung aus.
 */
void SensorMeasurementCycleManager::handleUnknownState() {
  handleStateError(F("Unbekannter Zustand aufgetreten"));
}

/**
 * @brief Zentrale Fehlerbehandlung bei Zustandsfehlern
 * @param error Beschreibung des Fehlers
 * @details Gibt den Messslot frei (falls gehalten), protokolliert den Fehler
 *          und setzt den Zustand auf ERROR.
 *
 * WICHTIG: Diese Funktion gibt den Slot frei und ruft recordError() auf.
 *          handleError() darf beides NICHT erneut tun.
 */
void SensorMeasurementCycleManager::handleStateError(const String& error) {
  MeasurementState previousState = m_state.state;
  m_lastState = previousState;

  // Slot freigeben, falls wir einen halten (nicht in Wartezuständen)
  if (previousState != MeasurementState::WAITING_FOR_DUE &&
      previousState != MeasurementState::WAITING_FOR_SLOT) {
    SensorManagerLimiter::getInstance().releaseSlot(m_sensor->getId());
    if (ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("MeasurementCycle"), m_sensor->getName() + F(": Slot wegen Fehler freigegeben"));
    }
  }

  // Fehler aufzeichnen (inkrementiert errorCount)
  m_state.recordError(error);

  // Zustand auf ERROR setzen
  m_state.setState(MeasurementState::ERROR, m_sensor->getName());

  LOG_ERROR(F("MeasurementCycle"), m_sensor->getName() + String(F(": Sensorfehler: ")) + error);
}

/**
 * @brief Plant den nächsten Versuch mit wachsendem Abstand
 * @details Siehe Erläuterung an der Deklaration. Der Sensor bleibt aktiviert;
 *          nur der Abstand wächst, gedeckelt auf MAX_RETRY_BACKOFF.
 */
void SensorMeasurementCycleManager::scheduleRetryWithBackoff() {
  if (!m_sensor) {
    return;
  }

  if (m_retryLevel < MAX_RETRY_LEVEL) {
    m_retryLevel++;
  }

  // Abstand aus dem reguläreren Messtakt verdoppeln, ohne überzulaufen
  unsigned long retryDelay = m_state.measurementInterval;
  if (retryDelay == 0) {
    retryDelay = ERROR_RETRY_DELAY;
  }
  for (uint8_t level = 1; level < m_retryLevel; level++) {
    if (retryDelay >= MAX_RETRY_BACKOFF / 2) {
      retryDelay = MAX_RETRY_BACKOFF;
      break;
    }
    retryDelay *= 2;
  }
  if (retryDelay > MAX_RETRY_BACKOFF) {
    retryDelay = MAX_RETRY_BACKOFF;
  }

  LOG_WARN(F("MeasurementCycle"), m_sensor->getName() +
                                      String(F(": Bleibt aktiv, nächster Versuch in ")) +
                                      String(retryDelay / 1000UL) + String(F(" s (Stufe ")) +
                                      String(m_retryLevel) + F(")"));

  // Frische Versuchsreihe für den nächsten Anlauf. hasPersistentError bleibt
  // gesetzt, damit der Zustand im Webinterface sichtbar ist; die erste
  // erfolgreiche Messung räumt beides wieder ab.
  m_state.errorCount = 0;
  m_state.fatalError = false;
  m_state.needsInitialization = true;
  m_state.scheduleNextMeasurement(millis(), retryDelay);
  m_state.setState(MeasurementState::WAITING_FOR_DUE, m_sensor->getName());
}
