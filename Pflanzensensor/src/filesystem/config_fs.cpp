/**
 * @file config_fs.cpp
 * @brief Implementation of dual LittleFS partition manager
 */

#include "config_fs.h"
#include "../logger/logger.h"
#include "../utils/critical_section.h"

DualFS::DualFS() : _configMounted(false), _mainMounted(false) {}

DualFS& DualFS::getInstance() {
  static DualFS instance;
  return instance;
}

ResourceResult DualFS::init() {
  logger.info(F("DualFS"), F("Initialisiere Dual-Partition-System..."));
  
  // CRITICAL: Mount CONFIG partition FIRST as global LittleFS
  // This ensures Preferences library uses CONFIG partition for storage
  auto configResult = mountConfigFS();
  if (!configResult.isSuccess()) {
    logger.warning(F("DualFS"), F("CONFIG mount fehlgeschlagen, formatiere..."));
    
    auto formatResult = formatConfigFS();
    if (!formatResult.isSuccess()) {
      logger.error(F("DualFS"), F("CONFIG formatieren fehlgeschlagen: ") + formatResult.getMessage());
      return formatResult;
    }
    
    configResult = mountConfigFS();
    if (!configResult.isSuccess()) {
      logger.error(F("DualFS"), F("CONFIG mount nach Format fehlgeschlagen: ") + configResult.getMessage());
      return configResult;
    }
  }
  
  // Log CONFIG filesystem information
  FSInfo configInfo;
  if (getConfigInfo(configInfo)) {
    logger.info(F("DualFS"), F("CONFIG Partition gemountet (für Preferences)"));
    logger.debug(F("DualFS"), F("CONFIG Gesamt: ") + String(configInfo.totalBytes) + F(" Bytes"));
    logger.debug(F("DualFS"), F("CONFIG Belegt: ") + String(configInfo.usedBytes) + F(" Bytes"));
  }
  
  // Now mount MAIN_FS partition for web assets
  auto mainResult = mountMainFS();
  if (!mainResult.isSuccess()) {
    logger.warning(F("DualFS"), F("MAIN_FS mount fehlgeschlagen, formatiere..."));
    
    auto formatResult = formatMainFS();
    if (!formatResult.isSuccess()) {
      logger.error(F("DualFS"), F("MAIN_FS formatieren fehlgeschlagen: ") + formatResult.getMessage());
      return formatResult;
    }
    
    mainResult = mountMainFS();
    if (!mainResult.isSuccess()) {
      logger.error(F("DualFS"), F("MAIN_FS mount nach Format fehlgeschlagen: ") + mainResult.getMessage());
      return mainResult;
    }
  }
  
  // Log MAIN_FS filesystem information
  FSInfo mainInfo;
  if (getMainInfo(mainInfo)) {
    logger.info(F("DualFS"), F("MAIN_FS Partition gemountet (für Web-Assets)"));
    logger.debug(F("DualFS"), F("MAIN_FS Gesamt: ") + String(mainInfo.totalBytes) + F(" Bytes"));
    logger.debug(F("DualFS"), F("MAIN_FS Belegt: ") + String(mainInfo.usedBytes) + F(" Bytes"));
  }
  
  logger.info(F("DualFS"), F("Dual-Partition-System erfolgreich initialisiert"));
  return ResourceResult::success();
}

ResourceResult DualFS::mountConfigFS() {
  if (_configMounted) {
    logger.debug(F("DualFS"), F("CONFIG bereits gemountet"));
    return ResourceResult::success();
  }
  
  CriticalSection cs;
  
  // Mount CONFIG partition as the GLOBAL LittleFS object
  // This is critical - Preferences library uses the global LittleFS
  LittleFSConfig cfg;
  cfg.setAutoFormat(false);
  
  // Configure for CONFIG partition
  if (!LittleFS.setConfig(LittleFSConfig(
      CONFIG_START,    // Start address from linker script
      CONFIG_SIZE,     // Partition size (64KB)
      256,             // Page size
      8192,            // Block size
      5                // Max open files
  ))) {
    String error = F("Fehler beim Konfigurieren der CONFIG Partition");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  if (!LittleFS.begin()) {
    String error = F("Fehler beim Mounten der CONFIG Partition");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  _configMounted = true;
  logger.debug(F("DualFS"), F("CONFIG Partition als globales LittleFS gemountet"));
  
  return ResourceResult::success();
}

ResourceResult DualFS::mountMainFS() {
  if (_mainMounted) {
    logger.debug(F("DualFS"), F("MAIN_FS bereits gemountet"));
    return ResourceResult::success();
  }
  
  CriticalSection cs;
  
  // Mount MAIN_FS partition as a separate LittleFS instance
  if (!_mainFS.setConfig(LittleFSConfig(
      MAIN_FS_START,   // Start address from linker script
      MAIN_FS_SIZE,    // Partition size (~844KB)
      256,             // Page size
      8192,            // Block size
      10               // Max open files (more for web serving)
  ))) {
    String error = F("Fehler beim Konfigurieren der MAIN_FS Partition");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  if (!_mainFS.begin()) {
    String error = F("Fehler beim Mounten der MAIN_FS Partition");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  _mainMounted = true;
  logger.debug(F("DualFS"), F("MAIN_FS Partition als separates Objekt gemountet"));
  
  return ResourceResult::success();
}

ResourceResult DualFS::formatConfigFS() {
  logger.info(F("DualFS"), F("Formatiere CONFIG Partition..."));
  
  CriticalSection cs;
  
  // Configure the partition before formatting
  if (!LittleFS.setConfig(LittleFSConfig(
      CONFIG_START,
      CONFIG_SIZE,
      256,
      8192,
      5
  ))) {
    String error = F("Fehler beim Konfigurieren für CONFIG Format");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  if (!LittleFS.format()) {
    String error = F("Fehler beim Formatieren der CONFIG Partition");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  logger.info(F("DualFS"), F("CONFIG Partition erfolgreich formatiert"));
  _configMounted = false; // Need to mount after format
  
  return ResourceResult::success();
}

ResourceResult DualFS::formatMainFS() {
  logger.info(F("DualFS"), F("Formatiere MAIN_FS Partition..."));
  
  CriticalSection cs;
  
  // Configure the partition before formatting
  if (!_mainFS.setConfig(LittleFSConfig(
      MAIN_FS_START,
      MAIN_FS_SIZE,
      256,
      8192,
      10
  ))) {
    String error = F("Fehler beim Konfigurieren für MAIN_FS Format");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  if (!_mainFS.format()) {
    String error = F("Fehler beim Formatieren der MAIN_FS Partition");
    logger.error(F("DualFS"), error);
    return ResourceResult::fail(ResourceError::FILESYSTEM_ERROR, error);
  }
  
  logger.info(F("DualFS"), F("MAIN_FS Partition erfolgreich formatiert"));
  _mainMounted = false; // Need to mount after format
  
  return ResourceResult::success();
}

bool DualFS::isConfigMounted() const {
  return _configMounted;
}

bool DualFS::isMainMounted() const {
  return _mainMounted;
}

fs::FS& DualFS::getMainFS() {
  return _mainFS;
}

bool DualFS::getConfigInfo(FSInfo& info) {
  if (!_configMounted) {
    logger.error(F("DualFS"), F("CONFIG nicht gemountet"));
    return false;
  }
  
  return LittleFS.info(info);
}

bool DualFS::getMainInfo(FSInfo& info) {
  if (!_mainMounted) {
    logger.error(F("DualFS"), F("MAIN_FS nicht gemountet"));
    return false;
  }
  
  return _mainFS.info(info);
}
