/**
 * @file manager_config_sensor_tracker.cpp
 * @brief Implementation of SensorErrorTracker class
 */

#include "manager_config_sensor_tracker.h"

SensorErrorTracker::SensorErrorTracker(ConfigNotifier& notifier)
    : m_notifier(notifier) {}
