/**
 * @file manager_config_notifier.cpp
 * @brief Implementation of ConfigNotifier class
 */

#include "manager_config_notifier.h"

#include "../logger/logger.h"

void ConfigNotifier::addChangeCallback(ChangeCallback callback) { m_callbacks.push_back(callback); }

void ConfigNotifier::notifyChange(const String& key, const String& value, bool updateSensors) {
  logger.info(F("ConfigN"), String(F("Config changed: ")) + key + F(" = ") + value +
                                F(", updateSensors=") + String(updateSensors));

  // Notify all registered callbacks
  for (const auto& callback : m_callbacks) {
    callback(key, value);
  }
}

void ConfigNotifier::clearCallbacks() { m_callbacks.clear(); }

size_t ConfigNotifier::getCallbackCount() const { return m_callbacks.size(); }
