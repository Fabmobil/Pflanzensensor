#include "managers/manager_sensor.h"

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor_persistence.h"

// Global instance
std::unique_ptr<SensorManager> sensorManager;

void SensorManager::updateMeasurements() {
  if (getState() != ManagerState::INITIALIZED) {
    return;
  }

  for (const auto& sensor : m_sensors) {
    if (!sensor || !sensor->isEnabled()) {
      continue;
    }

    auto& stateLog = m_sensorStates[sensor->getId()];
    auto cycleManager = m_cycleManagers[sensor->getId()].get();

    if (!cycleManager) {
      LOG_ERROR(F("SensorManager"), String(F("Kein Zyklusmanager für Sensor: ")) + sensor->getId());
      continue;
    }

    MeasurementState currentState = cycleManager->getCurrentState();
    unsigned long now = millis();

    bool stateChanged = (currentState != stateLog.lastState);
    stateLog.lastState = currentState;

    if (stateChanged && ConfigMgr.isDebugMeasurementCycle()) {
      LOG_DEBUG(F("SensorManager"), String(F("Sensor: ")) + sensor->getId() +
                                        String(F(" Zustand: ")) +
                                        String(static_cast<int>(currentState)) + F(" (geändert)"));
      stateLog.lastStateLogTime = now;
    }

    bool shouldProcess =
        (currentState == MeasurementState::WAITING_FOR_DUE && cycleManager->isDue()) ||
        (currentState != MeasurementState::WAITING_FOR_DUE);

    if (shouldProcess) {
      bool cycleResult = cycleManager->updateMeasurementCycle();
      bool resultChanged = (cycleResult != stateLog.lastUpdateResult);
      stateLog.lastUpdateResult = cycleResult;

      if (resultChanged && ConfigMgr.isDebugMeasurementCycle()) {
        LOG_DEBUG(F("SensorManager"),
                  String(F("Sensor: ")) + sensor->getId() + String(F(" Zyklus: ")) +
                      (cycleResult ? F("Abgeschlossen") : F("In Bearbeitung")) + F(" (geändert)"));
      }
    }

    yield();
  }
}

TypedResult<ResourceError, void> SensorManager::initialize() {
  auto result = SensorFactory::createAllSensors(m_sensors, this);
  if (!result.isSuccess() && !result.isPartialSuccess()) {
    return TypedResult<ResourceError, void>::fail(
        ResourceError::OPERATION_FAILED,
        String(F("Sensoren konnten nicht erstellt werden: ")) + result.getMessage());
  }

  if (result.isPartialSuccess()) {
    LOG_WARN(F("SensorM"), String(F("Einige Sensoren konnten nicht initialisiert werden: ")) +
                               result.getMessage());
  }

  bool hasFailedSensors = false;
  for (const auto& sensor : m_sensors) {
    if (sensor && sensor->config().hasPersistentError) {
      if (!sensor->isInitialized()) {
        LOG_DEBUG(
            F("SensorM"),
            String(F("Zuvor fehlgeschlagener Sensor ")) + sensor->getName() +
                F(" wurde während der Fabrikprüfung deinitialisiert, Fehlerflag wird entfernt"));
        sensor->mutableConfig().hasPersistentError = false;
        continue;
      }

      if (sensor->init().isSuccess()) {
        LOG_INFO(F("SensorM"), String(F("Zuvor fehlgeschlagener Sensor ")) + sensor->getName() +
                                   F(" ist nach Neustart wieder funktionsfähig"));
        sensor->mutableConfig().hasPersistentError = false;
      } else {
        LOG_ERROR(F("SensorM"), String(F("Zuvor fehlgeschlagener Sensor ")) + sensor->getName() +
                                    F(" ist nach Neustart weiterhin fehlerhaft"));
        sensor->stop();
        hasFailedSensors = true;
      }
    }
  }

  LOG_DEBUG(F("SensorM"), F("Überprüfe aktivierte Sensoren:"));
  for (const auto& sensor : m_sensors) {
    if (sensor) {
      String msg = F("Sensor-ID: ");
      msg += sensor->getId();
      msg += F(", Name: ");
      msg += sensor->getName();
      msg += F(", Aktiviert: ");
      msg += sensor->isEnabled() ? F("ja") : F("nein");
      LOG_DEBUG(F("SensorM"), msg);
    }
  }

  size_t enabledCount = 0;
  for (auto& sensor : m_sensors) {
    if (sensor && sensor->isEnabled()) {
      auto cycleManager = std::make_unique<SensorMeasurementCycleManager>(sensor.get());
      String sensorId = sensor->getId();
      m_cycleManagers[sensorId] = std::move(cycleManager);
      enabledCount++;
      LOG_DEBUG(F("SensorM"), String(F("Zyklusmanager für Sensor erstellt: ")) + sensorId);
    }
  }

  String msg = F("Es wurden ");
  msg += String(enabledCount);
  msg += F(" Zyklusmanager von insgesamt ");
  msg += String(m_sensors.size());
  msg += F(" Sensoren erstellt");
  LOG_DEBUG(F("SensorM"), msg);

  LOG_INFO(F("SensorM"), String(F("Initialisierung des Sensormanagers abgeschlossen mit ")) +
                             String(m_sensors.size()) + String(F(" Sensoren (")) +
                             String(enabledCount) + F(" aktiviert)"));

  setState(ManagerState::INITIALIZED);
  applySensorSettingsFromConfig();

  if (hasFailedSensors) {
    return TypedResult<ResourceError, void>::partialSuccess(
        F("Einige Sensoren sind nach dem Neustart weiterhin fehlerhaft"));
  }

  return TypedResult<ResourceError, void>::success();
}

void SensorManager::applySensorSettingsFromConfig() {
  if (ConfigMgr.isDebugSensor()) {
    LOG_DEBUG(F("SensorM"), F("Wende Sensoreinstellungen aus der Konfiguration an"));
  }

  LOG_INFO(F("SensorM"), F("Sensoreinstellungen aus der Konfiguration werden angewendet"));

  // Load sensor configuration from file
  auto result = SensorPersistence::load();
  if (!result.isSuccess()) {
    LOG_WARN(F("SensorM"),
             String(F("Sensor-Konfiguration konnte nicht geladen werden: ")) + result.getMessage());
    return;
  }

  if (ConfigMgr.isDebugSensor()) {
    LOG_DEBUG(F("SensorM"), F("Sensor-Konfiguration erfolgreich aus Datei geladen"));
  }

  LOG_INFO(F("SensorM"), F("Sensoreinstellungen erfolgreich angewendet"));
}
