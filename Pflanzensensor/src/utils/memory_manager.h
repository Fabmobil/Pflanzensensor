/**
 * @file memory_manager.h
 * @brief Memory safety utilities for ESP8266
 * @details Provides heap fragmentation monitoring, leak detection,
 *          and memory safety utilities for embedded systems.
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>
#include <functional>

/**
 * @struct MemoryMetrics
 * @brief Struct to hold memory statistics
 */
struct MemoryMetrics {
  uint32_t totalHeap;       ///< Total heap size
  uint32_t freeHeap;        ///< Free heap space
  uint32_t maxFreeBlock;    ///< Largest contiguous block
  uint32_t minFreeHeap;     ///< Minimum free heap since last reset
  uint32_t usedHeap;        ///< Heap in use
  uint8_t fragmentation;    ///< Heap fragmentation percentage
  uint32_t allocatedBlocks; ///< Number of allocated blocks
  uint32_t freeBlocks;      ///< Number of free blocks
};

/**
 * @class MemoryManager
 * @brief Singleton class for memory safety and monitoring
 * @details Provides memory tracking, leak detection, fragmentation
 *          monitoring, and safety checks for embedded systems.
 *          Implements RAII for automatic cleanup tracking.
 */
class MemoryManager {
public:
  /**
   * @brief Get singleton instance
   * @return Reference to MemoryManager instance
   */
  static MemoryManager& getInstance();

  /**
   * @brief Initialize memory manager
   * @return true if successful, false otherwise
   */
  bool init();

  /**
   * @brief Get current memory metrics
   * @return MemoryMetrics struct with current stats
   */
  MemoryMetrics getMetrics() const;

  /**
   * @brief Get minimum free heap since initialization
   * @return Minimum free heap value
   */
  uint32_t getMinFreeHeap() const { return m_minFreeHeap; }

  /**
   * @brief Get maximum fragmentation observed
   * @return Maximum fragmentation percentage
   */
  uint8_t getMaxFragmentation() const { return m_maxFragmentation; }

  /**
   * @brief Get the project name
   * @return Project name string
   */
  static const char* getProjectName() { return "pflanzensensor"; }

  /**
   * @brief Check if memory is critically low
   * @param threshold Threshold in bytes (default: 3000)
   * @return true if memory is below threshold
   */
  bool isCritical(uint32_t threshold = 3000) const { return getMetrics().freeHeap < threshold; }

  /**
   * @brief Check if fragmentation is problematic
   * @param threshold Threshold percentage (default: 50)
   * @return true if fragmentation exceeds threshold
   */
  bool isHighFragmentation(uint8_t threshold = 50) const {
    return getMetrics().fragmentation > threshold;
  }

  /**
   * @brief Check if safe to allocate memory
   * @param requiredBytes Required allocation size
   * @return true if sufficient contiguous block available
   */
  bool canAllocate(size_t requiredBytes) const {
    return getMetrics().maxFreeBlock >= requiredBytes && getMetrics().freeHeap >= requiredBytes * 2;
  }

  /**
   * @brief Add memory allocation tracking
   * @param bytes Number of bytes allocated
   * @return true if tracking successful
   */
  bool trackAllocation(size_t bytes);

  /**
   * @brief Remove memory allocation tracking
   * @param bytes Number of bytes freed
   * @return true if tracking successful
   */
  bool trackDeallocation(size_t bytes);

  /**
   * @brief Get total tracked allocations
   * @return Total bytes currently tracked as allocated
   */
  uint32_t getTrackedAllocations() const { return m_trackedAllocations; }

  /**
   * @brief Set callback for memory alerts
   * @param callback Function to call when alerts triggered
   */
  void setAlertCallback(std::function<void(const char*, uint32_t)> callback) {
    m_alertCallback = callback;
  }

  /**
   * @brief Trigger manual memory check
   * @return true if issues found
   */
  bool checkMemory();

  /**
   * @brief Log current memory state
   * @param message Optional message to include in log
   */
  void logState(const char* message = nullptr) const;

  /**
   * @brief Reset memory statistics
   * @details Called on system startup to reset tracking
   */
  void resetStats();

  /**
   * @brief Emergency cleanup - attempt to free memory
   * @return Bytes freed, or 0 if no memory available to free
   */
  uint32_t emergencyCleanup();

  /**
   * @brief Check heap and run emergency cleanup if needed
   * @param threshold Bytes threshold for cleanup (default: 4000)
   * @return true if cleanup was triggered
   */
  bool checkAndCleanup(uint32_t threshold = 4000);

private:
  MemoryManager() = default;
  ~MemoryManager() = default;
  MemoryManager(const MemoryManager&) = delete;
  MemoryManager& operator=(const MemoryManager&) = delete;

  uint32_t m_minFreeHeap;
  uint8_t m_maxFragmentation;
  uint32_t m_trackedAllocations;
  std::function<void(const char*, uint32_t)> m_alertCallback;
  bool m_initialized;

  void updateMetrics();
  void checkAndAlert(const char* alertType, uint32_t value);
};

extern MemoryManager& MemoryMgr;

#endif // MEMORY_MANAGER_H
