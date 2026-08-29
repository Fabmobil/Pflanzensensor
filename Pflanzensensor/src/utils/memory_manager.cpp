/**
 * @file memory_manager.cpp
 * @brief Memory safety utilities implementation
 */

#include "utils/memory_manager.h"
#include "logger/logger.h"

#if USE_WIFI
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#endif

#include <algorithm>
#include <vector>

MemoryManager& MemoryMgr = MemoryManager::getInstance();
static std::vector<std::pair<size_t, uint32_t>> g_allocationHistory;
static constexpr size_t MAX_HISTORY = 100;

MemoryManager& MemoryManager::getInstance() {
  static MemoryManager instance;
  return instance;
}

bool MemoryManager::init() {
  if (m_initialized)
    return true;

  resetStats();
  m_initialized = true;
  logger.info(F("MemMgr"), F("Memory manager initialized"));
  return true;
}

MemoryMetrics MemoryManager::getMetrics() const {
  MemoryMetrics metrics;
  metrics.freeHeap = ESP.getFreeHeap();
#ifdef ESP32
  metrics.maxFreeBlock = ESP.getMaxAllocHeap();
  metrics.totalHeap = ESP.getHeapSize();
#else
  metrics.maxFreeBlock = ESP.getMaxFreeBlockSize();
  // ESP8266 NodeMCU has 80KB RAM total
  metrics.totalHeap = 81920;
#endif
  metrics.minFreeHeap = m_minFreeHeap;
  metrics.usedHeap = metrics.totalHeap - metrics.freeHeap;

  if (metrics.freeHeap > 0) {
    metrics.fragmentation = static_cast<uint8_t>(
        100.0f -
        ((static_cast<float>(metrics.maxFreeBlock) / static_cast<float>(metrics.freeHeap)) *
         100.0f));
  } else {
    metrics.fragmentation = 0;
  }

  metrics.allocatedBlocks =
      static_cast<uint32_t>(std::count_if(g_allocationHistory.begin(), g_allocationHistory.end(),
                                          [](const auto& p) { return p.second > 0; }));
  metrics.freeBlocks =
      static_cast<uint32_t>(std::count_if(g_allocationHistory.begin(), g_allocationHistory.end(),
                                          [](const auto& p) { return p.second == 0; }));

  return metrics;
}

bool MemoryManager::trackAllocation(size_t bytes) {
  if (bytes == 0)
    return false;

  // Record allocation in history
  if (g_allocationHistory.size() >= MAX_HISTORY) {
    g_allocationHistory.erase(g_allocationHistory.begin());
  }
  g_allocationHistory.push_back({bytes, millis()});

  m_trackedAllocations += bytes;
  return true;
}

bool MemoryManager::trackDeallocation(size_t bytes) {
  if (bytes == 0 || m_trackedAllocations < bytes)
    return false;

  m_trackedAllocations -= bytes;
  return true;
}

void MemoryManager::resetStats() {
  m_minFreeHeap = ESP.getFreeHeap();
  m_maxFragmentation = 0;
  m_trackedAllocations = 0;
  g_allocationHistory.clear();

  logger.debug(F("MemMgr"),
               String(F("Memory stats reset - Heap: ")) + String(m_minFreeHeap) + F(" bytes"));
}

void MemoryManager::updateMetrics() {
  uint32_t currentFree = ESP.getFreeHeap();
  uint8_t currentFrag = 0;

  if (currentFree > 0) {
#ifdef ESP32
    uint32_t maxBlock = ESP.getMaxAllocHeap();
#else
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
#endif
    currentFrag = static_cast<uint8_t>(
        100.0f - ((static_cast<float>(maxBlock) / static_cast<float>(currentFree)) * 100.0f));
  }

  // Track minimum free heap
  m_minFreeHeap = std::min(m_minFreeHeap, currentFree);

  // Track maximum fragmentation
  m_maxFragmentation = std::max(m_maxFragmentation, currentFrag);
}

void MemoryManager::checkAndAlert(const char* alertType, uint32_t value) {
  if (m_alertCallback) {
    m_alertCallback(alertType, value);
  }
}

void MemoryManager::logState(const char* message) const {
  auto metrics = getMetrics();
  logger.debug(
      F("MemMgr"),
      String(F("Memory [")) + String(message ?: "stats") + String(F("]:")) +
          String(F("\n  Total Heap: ")) + String(metrics.totalHeap) + String(F(" bytes")) +
          String(F("\n  Free Heap: ")) + String(metrics.freeHeap) + String(F(" bytes")) +
          String(F("\n  Used Heap: ")) + String(metrics.usedHeap) + String(F(" bytes")) +
          String(F("\n  Max Block: ")) + String(metrics.maxFreeBlock) + String(F(" bytes")) +
          String(F("\n  Fragmentation: ")) + String(metrics.fragmentation) + String(F("%")) +
          String(F("\n  Min Free: ")) + String(metrics.minFreeHeap) + String(F(" bytes")) +
          String(F("\n  Tracked Allocs: ")) + String(m_trackedAllocations) + String(F(" bytes")));
}

bool MemoryManager::checkMemory() {
  auto metrics = getMetrics();
  bool issuesFound = false;

  // Check for critical memory
  if (metrics.freeHeap < 3000) {
    logger.warning(F("MemMgr"), F("CRITICAL: Free heap below 3KB!"));
    issuesFound = true;
    checkAndAlert("CRITICAL_HEAP", metrics.freeHeap);
  }

  // Check for high fragmentation
  if (metrics.fragmentation > 50) {
    logger.warning(F("MemMgr"), String(F("WARNING: High heap fragmentation ")) +
                                    String(metrics.fragmentation) + F("%"));
    issuesFound = true;
    checkAndAlert("HIGH_FRAGMENTATION", metrics.fragmentation);
  }

  // Check for memory leak pattern
  if (m_trackedAllocations > 10000 && m_trackedAllocations > metrics.usedHeap / 2) {
    logger.warning(F("MemMgr"), String(F("WARNING: Possible memory leak detected. Tracked: ")) +
                                    String(m_trackedAllocations) + F(" bytes"));
    issuesFound = true;
    checkAndAlert("POSSIBLE_LEAK", m_trackedAllocations);
  }

  return issuesFound;
}

uint32_t MemoryManager::emergencyCleanup() {
  // Force garbage collection
#ifndef ESP32
  ESP.wdtFeed();
#endif
  delay(10);

  // Clear allocation history
  uint32_t before = m_trackedAllocations;
  g_allocationHistory.clear();
  m_trackedAllocations = 0;

// Reconnect WiFi to free connection buffers
#if USE_WIFI
  WiFi.reconnect();
#endif

  delay(10);
#ifndef ESP32
  ESP.wdtFeed();
#endif

  return before;
}

bool MemoryManager::checkAndCleanup(uint32_t threshold) {
  uint32_t freeHeap = ESP.getFreeHeap();

  if (freeHeap < threshold) {
    logger.warning(F("MemMgr"), String(F("Niedriger Heap: ")) + String(freeHeap) +
                                    String(F(" Bytes (Limit: ")) + String(threshold) +
                                    F("), starte Notfall-Bereinigung..."));

    emergencyCleanup();

    uint32_t after = ESP.getFreeHeap();
    logger.info(F("MemMgr"), String(F("Nach Bereinigung: ")) + String(after) + F(" Bytes"));

    return true;
  }

  return false;
}
