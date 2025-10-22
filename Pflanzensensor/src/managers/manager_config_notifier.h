/**
 * @file manager_config_notifier.h
 * @brief Configuration change notification system
 */

#ifndef MANAGER_CONFIG_NOTIFIER_H
#define MANAGER_CONFIG_NOTIFIER_H

#include <Arduino.h>

#include <functional>
#include <vector>

class ConfigNotifier {
public:
  using ChangeCallback = std::function<void(const String&, const String&)>;

  /**
   * @brief Add a callback to be invoked on configuration changes
   * @param callback The callback function to be added
   */
  void addChangeCallback(ChangeCallback callback);

  /**
   * @brief Notify all registered callbacks of a configuration change
   * @param key The key of the changed configuration
   * @param value The new value of the configuration
   * @param updateSensors Whether to update sensor settings
   */
  void notifyChange(const String& key, const String& value, bool updateSensors = true);

  /**
   * @brief Clear all registered callbacks
   */
  void clearCallbacks();

  /**
   * @brief Get the number of registered callbacks
   * @return Number of registered callbacks
   */
  size_t getCallbackCount() const;

private:
  std::vector<ChangeCallback> m_callbacks;
};

#endif
