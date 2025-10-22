/**
 * @file manager_resource.h
 * @brief Resource Manager for handling memory and critical operations.
 */

#ifndef manager_resource_H
#define manager_resource_H

#include <Arduino.h>

#include <functional>
#include <memory>

#include "managers/manager_sensor.h"
#include "utils/result_types.h"

// Constants for memory management
/**
 * @brief Minimum free heap required for OTA updates.
 */
const uint32_t MIN_FREE_HEAP_FOR_OTA = 4096; // 4KB minimum free heap

/**
 * @brief Minimum contiguous block required for OTA updates.
 */
const uint32_t MIN_FREE_BLOCK_FOR_OTA = 4096; // 4KB minimum contiguous block

/**
 * @brief Memory thresholds for different operations
 */
struct MemoryThresholds {
  static const uint32_t CRITICAL_HEAP = 3000;  ///< Critical low memory threshold
  static const uint32_t WARNING_HEAP = 4000;   ///< Warning low memory threshold
  static const uint32_t SAFE_HEAP = 8000;      ///< Safe operating memory threshold
  static const uint8_t MAX_FRAGMENTATION = 50; ///< Maximum acceptable fragmentation percentage
};

/**
 * @class ResourceManager
 * @brief Singleton class for managing resources and critical operations.
 */
class ResourceManager {
private:
  static ResourceManager* instance;
  bool m_inCriticalOperation = false;
  String m_currentOperation;
  unsigned long m_criticalOperationStartTime = 0;
  unsigned long m_lastMemoryCheck = 0;
  static const unsigned long MEMORY_CHECK_INTERVAL = 10000; // 10 seconds
  uint8_t m_failureCount = 0;
  static const uint8_t MAX_FAILURES = 3;
  std::unique_ptr<SensorManager> m_sensorManager;

  // Private constructor for singleton
  ResourceManager() = default;

public:
  /**
   * @brief Get the singleton instance of ResourceManager.
   * @return Reference to the ResourceManager instance.
   */
  static ResourceManager& getInstance() {
    if (!instance) {
      instance = new ResourceManager();
    }
    return *instance;
  }

  using ResourceResult = TypedResult<ResourceError, void>;

  /**
   * @brief Execute a critical operation with memory management.
   * @param operation Name of the operation.
   * @param func Function to execute.
   * @return Result of the operation.
   */
  ResourceResult executeCritical(const String& operation, std::function<ResourceResult()> func);

  /**
   * @brief Enter a critical operation with memory checks.
   * @param operation Name of the operation.
   * @return Result of entering the critical operation.
   */
  ResourceResult enterCriticalOperation(const String& operation);

  /**
   * @brief Exit the current critical operation and cleanup.
   */
  void exitCriticalOperation();

  /**
   * @brief Initialize the minimal system.
   * @return Result of the initialization.
   */
  ResourceResult initMinimalSystem();

  /**
   * @brief Perform a firmware upgrade.
   * @return Result of the firmware upgrade.
   */
  ResourceResult doFirmwareUpgrade();

  /**
   * @brief Log the current memory status.
   * @param phase Current phase of the operation.
   */
  void logMemoryStatus(const String& phase);

  /**
   * @brief Perform emergency cleanup when memory is low.
   * @return True if cleanup was successful, false otherwise.
   */
  bool performEmergencyCleanup();

  /**
   * @brief Check if currently in a critical operation.
   * @return True if in a critical operation, false otherwise.
   */
  bool isInCriticalOperation() const { return m_inCriticalOperation; }

  /**
   * @brief Perform general cleanup of resources.
   */
  void cleanup();

  /**
   * @brief Check current memory status.
   * @return True if memory is in a safe state, false if action needed.
   */
  bool checkMemoryStatus();

  /**
   * @brief Get the current heap fragmentation percentage.
   * @return Current heap fragmentation as a percentage.
   */
  uint8_t getFragmentation() const {
    return static_cast<uint8_t>(100 - (ESP.getMaxFreeBlockSize() * 100.0 / ESP.getFreeHeap()));
  }

  /**
   * @brief Check if memory is critically low.
   * @return True if memory is critically low.
   */
  bool isCriticalMemory() const { return ESP.getFreeHeap() < MemoryThresholds::CRITICAL_HEAP; }

  /**
   * @brief Get the duration of the current critical operation.
   * @return Duration in milliseconds, 0 if no operation is active.
   */
  unsigned long getCriticalOperationDuration() const {
    return m_inCriticalOperation ? (millis() - m_criticalOperationStartTime) : 0;
  }

  /**
   * @brief Reset failure count after successful operation.
   */
  void resetFailureCount() { m_failureCount = 0; }

  /**
   * @brief Increment failure count and check if max failures reached.
   * @return True if max failures reached.
   */
  bool incrementFailureCount() { return ++m_failureCount >= MAX_FAILURES; }

  // Delete copy constructor and assignment operator
  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;
};

extern ResourceManager& ResourceMgr;

#endif // manager_resource_H
