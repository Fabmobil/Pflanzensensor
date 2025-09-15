#include "led.h"

#include "configs/config.h"
#include "logger/logger.h"

#if USE_LED_TRAFFIC_LIGHT

// led.cpp
ResourceResult LedLights::init() {
  logger.debug(F("LED"), F("Initialisiere LED-Pins"));

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);

  // Set all LEDs to off initially
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);

  return ResourceResult::success();
}

LedStatus LedLights::getStatus() const {
  LedStatus status;
  status.red = digitalRead(LED_RED_PIN) == HIGH;
  status.yellow = digitalRead(LED_YELLOW_PIN) == HIGH;
  status.green = digitalRead(LED_GREEN_PIN) == HIGH;
  return status;
}

ResourceResult LedLights::switchLedOn(int color) {
  if (!isValidColor(color)) {
    logger.warning(F("LED"), F("Ungültige LED-Farbe: ") + String(color));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                F("Ungültige LED-Farbe: ") + String(color));
  }

  switch (color) {
    case RED:
      digitalWrite(LED_RED_PIN, HIGH);
      break;
    case YELLOW:
      digitalWrite(LED_YELLOW_PIN, HIGH);
      break;
    case GREEN:
      digitalWrite(LED_GREEN_PIN, HIGH);
      break;
  }

  // logger.debug(F("LED"), F("LED ") + String(color) + F(" switched on"));
  return ResourceResult::success();
}

ResourceResult LedLights::switchLedOff(int color) {
  if (!isValidColor(color)) {
    logger.warning(F("LED"), F("Ungültige LED-Farbe: ") + String(color));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR,
                                F("Ungültige LED-Farbe: ") + String(color));
  }

  switch (color) {
    case RED:
      digitalWrite(LED_RED_PIN, LOW);
      break;
    case YELLOW:
      digitalWrite(LED_YELLOW_PIN, LOW);
      break;
    case GREEN:
      digitalWrite(LED_GREEN_PIN, LOW);
      break;
  }

  // logger.debug(F("LED"), F("LED ") + String(color) + F(" switched off"));
  return ResourceResult::success();
}

#endif  // USE_LED_TRAFFIC_LIGHT
