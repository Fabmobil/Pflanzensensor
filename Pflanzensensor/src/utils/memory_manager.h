/**
 * @file memory_manager.h
 * @brief Memory safety utilities for ESP8266
 * @details Provides heap fragmentation monitoring, leak detection,
 *          and memory safety utilities for embedded systems.
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>

/**
 * @struct MemoryMetrics
 * @brief Struct to hold memory statistics
 */
struct MemoryMetrics {
  uint32_t totalHeap;    ///< Total heap size
  uint32_t freeHeap;     ///< Free heap space
  uint32_t maxFreeBlock; ///< Largest contiguous block
  uint32_t minFreeHeap;  ///< Minimum free heap since last reset
  uint32_t usedHeap;     ///< Heap in use
  uint8_t fragmentation; ///< Heap fragmentation percentage
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
   * @brief Aufräumroutine registrieren, die bei Speichernot ausgeführt wird
   * @param handler Funktionszeiger; nullptr deaktiviert die Routine
   * @details Der WebManager registriert hier das Leeren seines Handler-Caches.
   *          Über den Zeiger bleibt der MemoryManager frei von Abhängigkeiten
   *          zur Web-Schicht.
   */
  using CleanupHandler = void (*)();
  void setCleanupHandler(CleanupHandler handler) { m_cleanupHandler = handler; }

  /**
   * @brief Notfall-Bereinigung bei knappem Heap
   * @return Tatsächlich freigegebene Bytes
   * @details Nicht-destruktiv: gibt ausschließlich Speicher frei, der sich
   *          jederzeit neu aufbauen lässt. WiFi-Verbindung und Sensorik bleiben
   *          unangetastet. Dies ist die einzige Notfall-Bereinigung im System;
   *          ResourceManager::performEmergencyCleanup() delegiert hierher.
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
  CleanupHandler m_cleanupHandler{nullptr};
  bool m_initialized;
};

extern MemoryManager& MemoryMgr;

#endif // MEMORY_MANAGER_H
