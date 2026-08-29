/**
 * @file manager_resource.cpp
 * @brief Resource manager implementation with singleton pattern
 */

#include "managers/manager_resource.h"

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "main_wifi.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/critical_section.h"
#include "utils/memory_manager.h"
#include "utils/wifi.h"
#include "web/core/web_manager.h"
#include "web/handler/web_ota_handler.h"

ResourceManager* ResourceManager::instance = nullptr;
ResourceManager& ResourceMgr = ResourceManager::getInstance();

ResourceResult ResourceManager::executeCritical(const String& operation,
                                                std::function<ResourceResult()> func) {
  auto status = enterCriticalOperation(operation);
  if (!status.isSuccess()) {
    return status;
  }

  auto result = func();
  exitCriticalOperation();
  return result;
}

ResourceResult ResourceManager::enterCriticalOperation(const String& operation) {
  // Track nesting level
  m_nestingLevel++;

  // Warning for nested critical operations
  if (m_nestingLevel > 1) {
    LOG_WARN(F("ResourceM"), String(F("Warnung: Kritische Operation verschachtelt (Level: ")) +
                                 String(m_nestingLevel) + String(F(", Operation: ")) + operation +
                                 String(F(", vorherige: ")) + m_currentOperation +
                                 F(") - Dies kann zu Deadlocks führen!"));
  }

  // Check if already in critical operation at same level
  if (m_inCriticalOperation && m_nestingLevel == 1) {
    m_nestingLevel--; // Reset nesting level before returning
    return ResourceResult::fail(ResourceError::ALREADY_IN_CRITICAL,
                                String(F("Bereits in einer kritischen Operation: ")) +
                                    m_currentOperation);
  }

  uint32_t freeHeap = ESP.getFreeHeap();
#ifdef ESP32
  uint32_t maxBlock = ESP.getMaxAllocHeap();
#else
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();
#endif

  if (freeHeap < MIN_FREE_HEAP_FOR_OTA || maxBlock < MIN_FREE_BLOCK_FOR_OTA) {
    LOG_WARN(F("ResourceM"), F("Wenig Speicher, versuche Bereinigung..."));

    if (!performEmergencyCleanup()) {
      return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                                  F("Konnte nicht genügend Speicher freigeben"));
    }

    freeHeap = ESP.getFreeHeap();
#ifdef ESP32
    maxBlock = ESP.getMaxAllocHeap();
#else
    maxBlock = ESP.getMaxFreeBlockSize();
#endif

    if (freeHeap < MIN_FREE_HEAP_FOR_OTA || maxBlock < MIN_FREE_BLOCK_FOR_OTA) {
      return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                                  F("Nicht genügend Speicher nach Bereinigung"));
    }
  }

  m_currentOperation = operation;
  m_inCriticalOperation = true;
  m_criticalOperationStartTime = millis();

  LOG_INFO(F("ResourceM"), String(F("Betrete kritische Operation: ")) + operation);

  return ResourceResult::success();
}

void ResourceManager::exitCriticalOperation() {
  if (!m_inCriticalOperation) {
    LOG_WARN(F("ResourceM"), F("Nicht in einer kritischen Operation"));
    return;
  }

  LOG_INFO(F("ResourceM"), String(F("Beende kritische Operation: ")) + m_currentOperation);

  // Verschachtelungsebene abbauen
  m_nestingLevel--;
  if (m_nestingLevel < 0) {
    m_nestingLevel = 0; // Sicherheitsnetz gegen negative Verschachtelung
    LOG_ERROR(F("ResourceM"), F("Fehler: Nesting-Level negativ! Möglicher Logic-Fehler"));
  }

  // Zustand erst auf oberster Ebene zurücksetzen.
  //
  // Vorher wurden m_inCriticalOperation und m_currentOperation am Ende der
  // Funktion bedingungslos zurückgesetzt. Damit war die gesamte darüber
  // stehende Nesting-Logik wirkungslos: schon das Verlassen einer inneren
  // Operation gab die äußere frei.
  if (m_nestingLevel == 0) {
    m_inCriticalOperation = false;
    m_currentOperation = "";
  }

  // Hier stand früher ein Block, der bei Bedarf einen ZWEITEN SensorManager
  // anlegte (ResourceManager::m_sensorManager parallel zum globalen
  // sensorManager). Das hätte sämtliche Sensorobjekte samt ihrer
  // Messkonfiguration doppelt im RAM gehalten und zwei Quellen der Wahrheit
  // erzeugt. Der Member ist entfernt; der globale sensorManager ist die
  // einzige Instanz.
}

ResourceResult ResourceManager::initMinimalSystem() {
  LOG_INFO(F("ResourceM"), F("Initialisiere minimales System..."));

  // Sensoren zuerst stoppen (globale Instanz - es gibt nur diese eine)
  extern std::unique_ptr<SensorManager> sensorManager;
  if (sensorManager) {
    LOG_DEBUG(F("ResourceM"), F("Stoppe alle Sensoren"));
    sensorManager->stopAll();
  }

#if USE_WEBSERVER
  WebManager::getInstance().stop(); // Replace server.stop()
#endif

  // Clear WiFi connections
#ifndef ESP32
  WiFiClient::stopAll();
#endif
  WiFi.persistent(false);
  WiFi.disconnect(true);

#ifndef ESP32
  ESP.wdtFeed();
#endif
  delay(200);
  yield();

  {
    if (!LittleFS.begin()) {
      return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR,
                                  F("Dateisystem konnte nicht eingehängt werden"));
    }
  }

#if USE_WIFI
  // Use consolidated WiFi+NTP setup
  LOG_INFO(F("ResourceM"), F(".. initialisiere WiFi"));
  auto wifiResult = setupWiFiWithDisplay(false);
  if (!wifiResult.isSuccess()) {
    return ResourceResult::fail(ResourceError::WIFI_ERROR,
                                String(F("WLAN-Verbindung konnte nicht hergestellt werden: ")) +
                                    wifiResult.getMessage());
  }
#endif

#if USE_WEBSERVER
  LOG_INFO(F("ResourceM"), F(".. initialisiere Webserver"));
  if (!WebManager::getInstance().begin()) { // Replace setupWebserver()
    LOG_ERROR(F("ResourceM"), F("WebManager konnte nicht initialisiert werden"));
    return ResourceResult::fail(ResourceError::WEBSERVER_INIT_FAILED);
  }
#endif

  delay(500);
  yield();

  return ResourceResult::success();
}

ResourceResult ResourceManager::doFirmwareUpgrade() {
  LOG_INFO(F("ResourceM"), F("Starte Firmware-Upgrade-Prozess..."));

  // Set firmware flag first
  auto configResult = ConfigMgr.setDoFirmwareUpgrade(true);
  if (!configResult.isSuccess()) {
    LOG_ERROR(F("ResourceM"),
              String(F("Setzen des Firmware-Flags fehlgeschlagen: ")) + configResult.getMessage());
    return ResourceResult::fail(ResourceError::OPERATION_FAILED,
                                String(F("Setzen des Firmware-Flags fehlgeschlagen: ")) +
                                    configResult.getMessage());
  }

  // Enter critical operation mode
  LOG_INFO(F("ResourceM"), F("Betrete kritischen Modus für Firmware-Upgrade"));
  auto status = enterCriticalOperation(F("Firmware Upgrade"));
  if (!status.isSuccess()) {
    LOG_ERROR(F("ResourceM"), F("Konnte kritischen Modus nicht betreten"));
    ConfigMgr.setDoFirmwareUpgrade(false);
    return status;
  }

  // Initialize minimal system
  LOG_INFO(F("ResourceM"), F("Initialisiere minimales System für Firmware-Upgrade"));
  auto initStatus = initMinimalSystem();
  if (!initStatus.isSuccess()) {
    LOG_ERROR(F("ResourceM"), F("Initialisierung des minimalen Systems fehlgeschlagen"));
    exitCriticalOperation();
    ConfigMgr.setDoFirmwareUpgrade(false);
    return initStatus;
  }

  // Give time for the system to stabilize
  delay(1000);

  LOG_INFO(F("ResourceM"), F("Vorbereitung für Firmware-Upgrade abgeschlossen, Neustart..."));
  return ResourceResult::success();
}

void ResourceManager::logMemoryStatus(const String& phase) {
  uint32_t freeHeap = ESP.getFreeHeap();
#ifdef ESP32
  uint32_t maxFreeBlock = ESP.getMaxAllocHeap();
#else
  uint32_t maxFreeBlock = ESP.getMaxFreeBlockSize();
#endif
  float fragmentation = 100.0f - ((float)maxFreeBlock / (float)freeHeap) * 100.0f;

  LOG_DEBUG(F("ResourceM"), String(F("Speicherstatistiken [")) + phase + String(F("]:")));
  LOG_DEBUG(F("ResourceM"), String(F("- Freier Heap: ")) + String(freeHeap) + String(F(" Bytes")));
  LOG_DEBUG(F("ResourceM"),
            String(F("- Größter freier Block: ")) + String(maxFreeBlock) + String(F(" Bytes")));
  LOG_DEBUG(F("ResourceM"),
            String(F("- Fragmentierung: ")) + String(fragmentation, 0) + String(F("%")));
#ifndef ESP32
  LOG_DEBUG(F("ResourceM"), String(F("- Freier Cont-Stack: ")) + String(ESP.getFreeContStack()) +
                                String(F(" Bytes")));
  LOG_DEBUG(F("ResourceM"), String(F("- Freier Stack: ")) +
                                String(ESP.getFreeHeap() - ESP.getHeapFragmentation()) +
                                String(F(" Bytes")));
#endif
}

void ResourceManager::cleanup() {
  // Stop all active operations
  if (m_inCriticalOperation) {
    exitCriticalOperation();
  }

  // Dienste zurücksetzen (globale Sensor-Instanz)
  extern std::unique_ptr<SensorManager> sensorManager;
  if (sensorManager) {
    LOG_DEBUG(F("ResourceM"), F("Beende Sensor-Manager"));
    sensorManager->stopAll();
  }

  // Clear memory
#ifndef ESP32
  ESP.wdtFeed();
#endif
  delay(100);

  // Force garbage collection
#ifndef ESP32
  ESP.wdtFeed();
#endif
  delay(100);

  // Log memory status
  logMemoryStatus("after cleanup");
}

bool ResourceManager::performEmergencyCleanup() {
  // Delegiert an die einzige Notfall-Bereinigung des Systems.
  //
  // Diese Funktion hatte früher eine eigene, deutlich destruktivere
  // Implementierung: sie stoppte alle Sensoren, zerstörte den SensorManager
  // und trennte WiFi, um es danach neu zu verbinden. Ausgelöst wurde das bei
  // knappem Heap - also genau dann, wenn laufende Sensorik und eine stabile
  // Verbindung am wichtigsten sind. Nach so einer "Bereinigung" lieferte das
  // Gerät bis zum nächsten Neustart keine Messwerte mehr.
  uint32_t freed = MemoryMgr.emergencyCleanup();

  LOG_INFO(F("ResourceM"),
           String(F("Notfall-Bereinigung abgeschlossen, ")) + String(freed) + F(" Bytes frei"));
  return true;
}
