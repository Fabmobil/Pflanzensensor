/**
 * @file manager_resource.cpp
 * @brief Resource manager implementation with singleton pattern
 */

#include "managers/manager_resource.h"

#include <ESP8266WiFi.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/critical_section.h"
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

  try {
    auto result = func();
    exitCriticalOperation();
    return result;
  } catch (const std::exception& e) {
    exitCriticalOperation();
    return ResourceResult::fail(ResourceError::OPERATION_FAILED,
                                String(F("Ausnahme: ")) + e.what());
  }
}

ResourceResult ResourceManager::enterCriticalOperation(const String& operation) {
  if (m_inCriticalOperation) {
    return ResourceResult::fail(ResourceError::ALREADY_IN_CRITICAL,
                                F("Bereits in einer kritischen Operation: ") + m_currentOperation);
  }

  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxBlock = ESP.getMaxFreeBlockSize();

  if (freeHeap < MIN_FREE_HEAP_FOR_OTA || maxBlock < MIN_FREE_BLOCK_FOR_OTA) {
    logger.warning(F("ResourceM"), F("Wenig Speicher, versuche Bereinigung..."));

    if (!performEmergencyCleanup()) {
      return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                                  F("Konnte nicht genügend Speicher freigeben"));
    }

    freeHeap = ESP.getFreeHeap();
    maxBlock = ESP.getMaxFreeBlockSize();

    if (freeHeap < MIN_FREE_HEAP_FOR_OTA || maxBlock < MIN_FREE_BLOCK_FOR_OTA) {
      return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                                  F("Nicht genügend Speicher nach Bereinigung"));
    }
  }

  m_currentOperation = operation;
  m_inCriticalOperation = true;
  m_criticalOperationStartTime = millis();

  logger.info(F("ResourceM"), F("Betrete kritische Operation: ") + operation);

  return ResourceResult::success();
}

void ResourceManager::exitCriticalOperation() {
  if (!m_inCriticalOperation) {
    logger.warning(F("ResourceM"), F("Nicht in einer kritischen Operation"));
    return;
  }

  logger.info(F("ResourceM"), String(F("Beende kritische Operation: ")) + m_currentOperation);

  // Only recreate sensor manager if we're not doing a firmware upgrade
  if (!ConfigMgr.getDoFirmwareUpgrade()) {
    // Recreate and initialize sensor manager if it was reset
    if (!m_sensorManager) {
      logger.debug(F("ResourceM"), F("Sensor-Manager neu erstellen"));
      try {
        m_sensorManager = std::make_unique<SensorManager>();
        if (m_sensorManager) {
          // Use the public init() method from Manager base class
          auto initResult = m_sensorManager->init();
          if (initResult.isSuccess()) {
            logger.info(F("ResourceM"), F("Sensor-Manager erfolgreich reinitialisiert"));
          } else {
            logger.error(F("ResourceM"),
                         String(F("Reinitialisierung des Sensor-Managers fehlgeschlagen: ")) +
                             initResult.getMessage());
            m_sensorManager.reset(); // Clean up on failure
          }
        } else {
          logger.error(F("ResourceM"), F("Zuweisung des Sensor-Managers fehlgeschlagen"));
        }
      } catch (const std::exception& e) {
        logger.error(F("ResourceM"),
                     String(F("Ausnahme bei Erstellung des Sensor-Managers: ")) + e.what());
        m_sensorManager.reset();
      }
    }

#if USE_WEBSERVER
    // WebManager is already initialized, no need to call begin() again
#endif
  }

  // Clear operation info
  m_inCriticalOperation = false;
  m_currentOperation = "";
}

ResourceResult ResourceManager::initMinimalSystem() {
  logger.info(F("ResourceM"), F("Initialisiere minimales System..."));

  // Stop all sensors first
  if (m_sensorManager) {
    logger.debug(F("ResourceM"), F("Stopping all sensors"));
    m_sensorManager->stopAll();
    m_sensorManager.reset();
  }

#if USE_WEBSERVER
  WebManager::getInstance().stop(); // Replace server.stop()
#endif

  // Clear WiFi connections
  WiFiClient::stopAll();
  WiFi.persistent(false);
  WiFi.disconnect(true);

  ESP.wdtFeed();
  delay(200);
  yield();

  {
    CriticalSection cs;
    if (!LittleFS.begin()) {
      return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR,
                                  F("Dateisystem konnte nicht eingehängt werden"));
    }
  }

#if USE_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  setupWiFi();

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return ResourceResult::fail(ResourceError::WIFI_ERROR,
                                F("WLAN-Verbindung konnte nicht hergestellt werden"));
  }

  logger.initNTP();
  logger.updateNTP();
#endif

#if USE_WEBSERVER
  logger.info(F("ResourceM"), F(".. initialisiere Webserver"));
  if (!WebManager::getInstance().begin()) { // Replace setupWebserver()
    logger.error(F("ResourceM"), F("WebManager konnte nicht initialisiert werden"));
    return ResourceResult::fail(ResourceError::WEBSERVER_INIT_FAILED);
  }
#endif

  delay(500);
  yield();

  return ResourceResult::success();
}

ResourceResult ResourceManager::doFirmwareUpgrade() {
  logger.info(F("ResourceM"), F("Starte Firmware-Upgrade-Prozess..."));

  // Set firmware flag first
  auto configResult = ConfigMgr.setDoFirmwareUpgrade(true);
  if (!configResult.isSuccess()) {
    logger.error(F("ResourceM"), String(F("Setzen des Firmware-Flags fehlgeschlagen: ")) +
                                     configResult.getMessage());
    return ResourceResult::fail(ResourceError::OPERATION_FAILED,
                                String(F("Setzen des Firmware-Flags fehlgeschlagen: ")) +
                                    configResult.getMessage());
  }

  // Enter critical operation mode
  logger.info(F("ResourceM"), F("Betrete kritischen Modus für Firmware-Upgrade"));
  auto status = enterCriticalOperation(F("Firmware Upgrade"));
  if (!status.isSuccess()) {
    logger.error(F("ResourceM"), F("Konnte kritischen Modus nicht betreten"));
    ConfigMgr.setDoFirmwareUpgrade(false);
    return status;
  }

  // Initialize minimal system
  logger.info(F("ResourceM"), F("Initialisiere minimales System für Firmware-Upgrade"));
  auto initStatus = initMinimalSystem();
  if (!initStatus.isSuccess()) {
    logger.error(F("ResourceM"), F("Initialisierung des minimalen Systems fehlgeschlagen"));
    exitCriticalOperation();
    ConfigMgr.setDoFirmwareUpgrade(false);
    return initStatus;
  }

  // Give time for the system to stabilize
  delay(1000);

  logger.info(F("ResourceM"), F("Vorbereitung für Firmware-Upgrade abgeschlossen, Neustart..."));
  return ResourceResult::success();
}

void ResourceManager::logMemoryStatus(const String& phase) {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t maxFreeBlock = ESP.getMaxFreeBlockSize();
  float fragmentation = 100.0f - ((float)maxFreeBlock / (float)freeHeap) * 100.0f;

  logger.debug(F("ResourceM"), F("Speicherstatistiken [") + phase + F("]:"));
  logger.debug(F("ResourceM"), F("- Freier Heap: ") + String(freeHeap) + F(" Bytes"));
  logger.debug(F("ResourceM"), F("- Größter freier Block: ") + String(maxFreeBlock) + F(" Bytes"));
  logger.debug(F("ResourceM"), F("- Fragmentierung: ") + String(fragmentation, 0) + F("%"));
  logger.debug(F("ResourceM"),
               F("- Freier Cont-Stack: ") + String(ESP.getFreeContStack()) + F(" Bytes"));
  logger.debug(F("ResourceM"), F("- Freier Stack: ") +
                                   String(ESP.getFreeHeap() - ESP.getHeapFragmentation()) +
                                   F(" Bytes"));
}

void ResourceManager::cleanup() {
  // Stop all active operations
  if (m_inCriticalOperation) {
    exitCriticalOperation();
  }

  // Reset all services
  if (m_sensorManager) {
    logger.debug(F("ResourceM"), F("Beende Sensor-Manager"));
    m_sensorManager->stopAll();
    m_sensorManager.reset();
  }

  // Clear memory
  ESP.wdtFeed();
  delay(100);

  // Force garbage collection
  ESP.wdtFeed();
  delay(100);

  // Log memory status
  logMemoryStatus("after cleanup");
}

bool ResourceManager::performEmergencyCleanup() {
  logger.warning(F("ResourceM"), F("Führe Notfall-Bereinigung durch..."));

  // Stop all sensors
  if (m_sensorManager) {
    m_sensorManager->stopAll();
    m_sensorManager.reset();
  }

  // Disconnect WiFi temporarily
  WiFi.disconnect(true);
  delay(100);

  // Clear any pending operations
  m_inCriticalOperation = false;
  m_currentOperation = "";

  // Force garbage collection
  ESP.wdtFeed();
  delay(100);

  // Reconnect WiFi
  WiFi.reconnect();
  delay(100);

  logger.info(F("ResourceM"), F("Notfall-Bereinigung abgeschlossen"));
  return true;
}
