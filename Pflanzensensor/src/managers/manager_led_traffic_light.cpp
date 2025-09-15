/**
 * @file manager_led_traffic_light.cpp
 * @brief LED traffic light manager implementation
 */

#include "managers/manager_led_traffic_light.h"

#if USE_LED_TRAFFIC_LIGHT

#include <stdio.h>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"

extern Logger logger;

TypedResult<ResourceError, void> LedTrafficLightManager::initialize() {
#if USE_LED_TRAFFIC_LIGHT
  logger.debug(F("LedTrafficLight"), F("Initialisiere LedTrafficLightManager"));

  m_ledLights = std::make_unique<LedLights>();
  if (!m_ledLights) {
    logger.warning(F("LedTrafficLight"),
                   F("LED-Ampel Zuweisung fehlgeschlagen"));
    return TypedResult<ResourceError, void>::fail(
        ResourceError::OPERATION_FAILED, F("Zuweisung der LED-Ampel fehlgeschlagen"));
  }

  auto initResult = m_ledLights->init();
  if (!initResult.isSuccess()) {
  logger.warning(F("LedTrafficLight"),
           F("Initialisierung der LED-Ampel fehlgeschlagen: ") +
             initResult.getMessage());
  return TypedResult<ResourceError, void>::fail(
    ResourceError::OPERATION_FAILED,
    F("Initialisierung der LED-Ampel fehlgeschlagen: ") + initResult.getMessage());
  }

  logger.info(F("LedTrafficLight"),
              F("LedTrafficLightManager erfolgreich initialisiert"));
  return TypedResult<ResourceError, void>::success();
#else
  logger.debug(F("LedTrafficLight"),
               F("LED traffic light disabled, skipping initialization"));
  return TypedResult<ResourceError, void>::success();
#endif
}

void LedTrafficLightManager::setStatus(const String& status) {
#if USE_LED_TRAFFIC_LIGHT
  if (!m_ledLights) return;

  // Don't update if status hasn't changed
  if (m_lastStatus == status) {
    return;
  }

  // Set the LED status based on the string
  if (status == "red") {
    m_ledLights->switchLedOn(LedLights::RED);
    m_ledLights->switchLedOff(LedLights::YELLOW);
    m_ledLights->switchLedOff(LedLights::GREEN);
  } else if (status == "yellow") {
    m_ledLights->switchLedOff(LedLights::RED);
    m_ledLights->switchLedOn(LedLights::YELLOW);
    m_ledLights->switchLedOff(LedLights::GREEN);
  } else if (status == "green") {
    m_ledLights->switchLedOff(LedLights::RED);
    m_ledLights->switchLedOff(LedLights::YELLOW);
    m_ledLights->switchLedOn(LedLights::GREEN);
  } else {
    // Unknown status, turn off all LEDs
    m_ledLights->switchLedOff(LedLights::RED);
    m_ledLights->switchLedOff(LedLights::YELLOW);
    m_ledLights->switchLedOff(LedLights::GREEN);
  }

  m_lastStatus = status;
#endif
}

void LedTrafficLightManager::setMeasurementStatus(const String& measurementId,
                                                  const String& status) {
#if USE_LED_TRAFFIC_LIGHT
  // Only update if we're in mode 2 and this is the selected measurement
  if (ConfigMgr.getLedTrafficLightMode() == 2 &&
      ConfigMgr.getLedTrafficLightSelectedMeasurement() == measurementId) {
    setStatus(status);
  }
#else
  // LED traffic light disabled, do nothing
#endif
}

uint8_t LedTrafficLightManager::getMode() const {
  return ConfigMgr.getLedTrafficLightMode();
}

String LedTrafficLightManager::getSelectedMeasurement() const {
  return ConfigMgr.getLedTrafficLightSelectedMeasurement();
}

void LedTrafficLightManager::turnOffAllLeds() {
#if USE_LED_TRAFFIC_LIGHT
  if (!m_ledLights) return;

  // Turn off all LEDs
  m_ledLights->switchLedOff(LedLights::RED);
  m_ledLights->switchLedOff(LedLights::YELLOW);
  m_ledLights->switchLedOff(LedLights::GREEN);
#endif
}

void LedTrafficLightManager::updateSelectedMeasurementStatus() {
#if USE_LED_TRAFFIC_LIGHT
  // Only update if we're in mode 2 and have a selected measurement
  if (ConfigMgr.getLedTrafficLightMode() == 2 &&
      !ConfigMgr.getLedTrafficLightSelectedMeasurement().isEmpty()) {
    // Parse the selected measurement identifier
    String selectedId = ConfigMgr.getLedTrafficLightSelectedMeasurement();
    int underscorePos = selectedId.indexOf('_');
    if (underscorePos > 0) {
      String sensorId = selectedId.substring(0, underscorePos);
      int measurementIndex = selectedId.substring(underscorePos + 1).toInt();

      // Find the sensor and get its current status
      extern std::unique_ptr<SensorManager> sensorManager;
      if (sensorManager) {
        for (const auto& sensor : sensorManager->getSensors()) {
          if (sensor && sensor->getId() == sensorId && sensor->isEnabled()) {
            // Get the current status for this measurement
            String status = sensor->getStatus(measurementIndex);
            if (!status.isEmpty()) {
              setStatus(status);
              return;
            }
          }
        }
      }
    }

    // If we can't get the status, turn off the LED
    turnOffAllLeds();
  }
#endif
}

void LedTrafficLightManager::handleDisplayUpdate() {
#if USE_LED_TRAFFIC_LIGHT
  uint8_t mode = ConfigMgr.getLedTrafficLightMode();

  if (mode == 0) {
    // Mode 0: LED traffic light off - do nothing
    return;
  } else if (mode == 1) {
    // Mode 1: Turn off LEDs when non-measurement screens are shown
    turnOffAllLeds();
  }
  // Mode 2: Do nothing - keep showing selected measurement status
#endif
}

#endif  // USE_LED_TRAFFIC_LIGHT
