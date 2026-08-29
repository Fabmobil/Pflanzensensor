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

MemoryManager& MemoryMgr = MemoryManager::getInstance();

MemoryManager& MemoryManager::getInstance() {
  static MemoryManager instance;
  return instance;
}

bool MemoryManager::init() {
  if (m_initialized)
    return true;

  resetStats();
  m_initialized = true;
  LOG_INFO(F("MemMgr"), F("Memory manager initialized"));
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

  return metrics;
}

void MemoryManager::resetStats() {
  m_minFreeHeap = ESP.getFreeHeap();
  m_maxFragmentation = 0;

  LOG_DEBUG(F("MemMgr"),
            String(F("Memory stats reset - Heap: ")) + String(m_minFreeHeap) + F(" bytes"));
}

void MemoryManager::logState(const char* message) const {
  auto metrics = getMetrics();
  LOG_DEBUG(F("MemMgr"),
            String(F("Memory [")) + String(message ?: "stats") + String(F("]:")) +
                String(F("\n  Total Heap: ")) + String(metrics.totalHeap) + String(F(" bytes")) +
                String(F("\n  Free Heap: ")) + String(metrics.freeHeap) + String(F(" bytes")) +
                String(F("\n  Used Heap: ")) + String(metrics.usedHeap) + String(F(" bytes")) +
                String(F("\n  Max Block: ")) + String(metrics.maxFreeBlock) + String(F(" bytes")) +
                String(F("\n  Fragmentation: ")) + String(metrics.fragmentation) + String(F("%")) +
                String(F("\n  Min Free: ")) + String(metrics.minFreeHeap) + String(F(" bytes")));
}

bool MemoryManager::checkMemory() {
  auto metrics = getMetrics();
  bool issuesFound = false;

  // Check for critical memory
  if (metrics.freeHeap < 3000) {
    LOG_WARN(F("MemMgr"), F("CRITICAL: Free heap below 3KB!"));
    issuesFound = true;
  }

  // Check for high fragmentation
  if (metrics.fragmentation > 50) {
    LOG_WARN(F("MemMgr"), String(F("WARNING: High heap fragmentation ")) +
                              String(metrics.fragmentation) + F("%"));
    issuesFound = true;
  }

  return issuesFound;
}

uint32_t MemoryManager::emergencyCleanup() {
  // Einzige Notfall-Bereinigung des Systems.
  //
  // Es gab hier früher zwei konkurrierende Implementierungen - diese und
  // ResourceManager::performEmergencyCleanup() - die beide destruktiv waren:
  // die eine rief WiFi.reconnect(), die andere trennte WiFi komplett UND
  // zerstörte den SensorManager. Beide wurden ausgelöst, wenn der Heap knapp
  // wurde, also genau dann, wenn eine stabile Verbindung und laufende Sensorik
  // am wichtigsten sind. Ein voller Reconnect kostet zudem selbst erst einmal
  // Heap und dauert Sekunden.
  //
  // Die Bereinigung ist deshalb jetzt nicht-destruktiv: sie gibt nur Speicher
  // frei, der sich jederzeit neu aufbauen lässt. Netzwerk und Sensorik bleiben
  // unangetastet.
  const uint32_t before = ESP.getFreeHeap();

#ifndef ESP32
  ESP.wdtFeed();
#endif

  // Registrierte Aufräumroutine ausführen (der WebManager gibt hier seinen
  // Handler-Cache frei - das ist der mit Abstand größte freigebbare Block).
  if (m_cleanupHandler) {
    m_cleanupHandler();
  }

#ifndef ESP32
  ESP.wdtFeed();
#endif

  const uint32_t after = ESP.getFreeHeap();
  return (after > before) ? (after - before) : 0;
}

bool MemoryManager::checkAndCleanup(uint32_t threshold) {
  uint32_t freeHeap = ESP.getFreeHeap();

  if (freeHeap < threshold) {
    LOG_WARN(F("MemMgr"), String(F("Niedriger Heap: ")) + String(freeHeap) +
                              String(F(" Bytes (Limit: ")) + String(threshold) +
                              F("), starte Notfall-Bereinigung..."));

    uint32_t freed = emergencyCleanup();

    LOG_INFO(F("MemMgr"), String(F("Nach Bereinigung: ")) + String(ESP.getFreeHeap()) +
                              String(F(" Bytes (")) + String(freed) + F(" freigegeben)"));

    return true;
  }

  return false;
}
