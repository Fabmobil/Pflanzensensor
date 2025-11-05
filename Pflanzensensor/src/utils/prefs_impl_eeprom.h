/**
 * @file prefs_impl_eeprom.h
 * @brief EEPROM-based implementation for Preferences library
 * @details This provides an EEPROM backend for the vshymanskyy/Preferences library
 *          specifically for ESP8266, storing data in the dedicated EEPROM partition
 *          (0x405F7000, 16KB) which survives filesystem updates.
 */

#pragma once

#include <EEPROM.h>

// EEPROM configuration
#define PREFS_EEPROM_SIZE 4096  // Use 4KB of 16KB available
#define PREFS_EEPROM_MAGIC 0x5072  // "Pr" in hex
#define PREFS_EEPROM_VERSION 1

// Directory entry for namespace tracking
struct EEPROMNamespaceEntry {
  char name[16];
  uint16_t offset;
  uint16_t size;
  uint8_t valid;
  uint8_t reserved[3];
};

// EEPROM layout
#define EEPROM_HEADER_SIZE 16
#define EEPROM_DIR_OFFSET EEPROM_HEADER_SIZE
#define EEPROM_MAX_NAMESPACES 32
#define EEPROM_DIR_SIZE (EEPROM_MAX_NAMESPACES * sizeof(EEPROMNamespaceEntry))
#define EEPROM_DATA_OFFSET (EEPROM_DIR_OFFSET + EEPROM_DIR_SIZE)
#define EEPROM_DATA_SIZE (PREFS_EEPROM_SIZE - EEPROM_DATA_OFFSET)

static bool _eeprom_initialized = false;

static bool _fs_init() {
  if (!_eeprom_initialized) {
    EEPROM.begin(PREFS_EEPROM_SIZE);
    
    // Check if EEPROM is initialized
    uint16_t magic = 0;
    EEPROM.get(0, magic);
    
    if (magic != PREFS_EEPROM_MAGIC) {
      // Initialize EEPROM
      EEPROM.put(0, PREFS_EEPROM_MAGIC);
      uint8_t version = PREFS_EEPROM_VERSION;
      EEPROM.put(2, version);
      
      // Clear directory
      for (int i = 0; i < EEPROM_MAX_NAMESPACES; i++) {
        EEPROMNamespaceEntry entry = {0};
        EEPROM.put(EEPROM_DIR_OFFSET + i * sizeof(EEPROMNamespaceEntry), entry);
      }
      
      EEPROM.commit();
    }
    
    _eeprom_initialized = true;
  }
  return true;
}

static bool _fs_mkdir(const char *path) {
  // Not needed for EEPROM
  return true;
}

static int findNamespace(const char* path) {
  // Extract namespace name from path (format: "/namespace/key")
  const char* ns_start = path + 1;  // Skip leading '/'
  const char* ns_end = strchr(ns_start, '/');
  
  if (!ns_end) {
    return -1;
  }
  
  char namespace_name[16] = {0};
  size_t len = min((size_t)(ns_end - ns_start), sizeof(namespace_name) - 1);
  strncpy(namespace_name, ns_start, len);
  
  // Search for namespace in directory
  for (int i = 0; i < EEPROM_MAX_NAMESPACES; i++) {
    EEPROMNamespaceEntry entry;
    EEPROM.get(EEPROM_DIR_OFFSET + i * sizeof(EEPROMNamespaceEntry), entry);
    
    if (entry.valid && strcmp(entry.name, namespace_name) == 0) {
      return i;
    }
  }
  
  return -1;
}

static int createNamespace(const char* namespace_name) {
  // Find free slot
  for (int i = 0; i < EEPROM_MAX_NAMESPACES; i++) {
    EEPROMNamespaceEntry entry;
    EEPROM.get(EEPROM_DIR_OFFSET + i * sizeof(EEPROMNamespaceEntry), entry);
    
    if (!entry.valid) {
      // Found free slot
      strncpy(entry.name, namespace_name, sizeof(entry.name) - 1);
      entry.offset = EEPROM_DATA_OFFSET + (i * 128);  // Allocate 128 bytes per namespace
      entry.size = 128;
      entry.valid = 1;
      
      EEPROM.put(EEPROM_DIR_OFFSET + i * sizeof(EEPROMNamespaceEntry), entry);
      EEPROM.commit();
      
      return i;
    }
  }
  
  return -1;  // No free slots
}

static int _fs_create(const char* path, const void* buf, int bufsize) {
  // Extract namespace from path
  const char* ns_start = path + 1;
  const char* ns_end = strchr(ns_start, '/');
  
  if (!ns_end) {
    return -1;
  }
  
  char namespace_name[16] = {0};
  size_t len = min((size_t)(ns_end - ns_start), sizeof(namespace_name) - 1);
  strncpy(namespace_name, ns_start, len);
  
  // Find or create namespace
  int ns_idx = findNamespace(path);
  if (ns_idx < 0) {
    ns_idx = createNamespace(namespace_name);
    if (ns_idx < 0) {
      return -1;
    }
  }
  
  // Get namespace entry
  EEPROMNamespaceEntry entry;
  EEPROM.get(EEPROM_DIR_OFFSET + ns_idx * sizeof(EEPROMNamespaceEntry), entry);
  
  // Write data to EEPROM
  if (bufsize > entry.size) {
    bufsize = entry.size;  // Truncate if too large
  }
  
  for (int i = 0; i < bufsize; i++) {
    EEPROM.write(entry.offset + i, ((uint8_t*)buf)[i]);
  }
  
  EEPROM.commit();
  
  return bufsize;
}

static int _fs_read(const char* path, void* buf, int bufsize) {
  int ns_idx = findNamespace(path);
  if (ns_idx < 0) {
    return -1;
  }
  
  EEPROMNamespaceEntry entry;
  EEPROM.get(EEPROM_DIR_OFFSET + ns_idx * sizeof(EEPROMNamespaceEntry), entry);
  
  if (bufsize > entry.size) {
    bufsize = entry.size;
  }
  
  for (int i = 0; i < bufsize; i++) {
    ((uint8_t*)buf)[i] = EEPROM.read(entry.offset + i);
  }
  
  return bufsize;
}

static int _fs_get_size(const char* path) {
  int ns_idx = findNamespace(path);
  if (ns_idx < 0) {
    return -1;
  }
  
  EEPROMNamespaceEntry entry;
  EEPROM.get(EEPROM_DIR_OFFSET + ns_idx * sizeof(EEPROMNamespaceEntry), entry);
  
  return entry.size;
}

static bool _fs_exists(const char* path) {
  return findNamespace(path) >= 0;
}

static bool _fs_rename(const char* from, const char* to) {
  // Not commonly used, can implement if needed
  return false;
}

static bool _fs_unlink(const char* path) {
  int ns_idx = findNamespace(path);
  if (ns_idx < 0) {
    return false;
  }
  
  EEPROMNamespaceEntry entry;
  EEPROM.get(EEPROM_DIR_OFFSET + ns_idx * sizeof(EEPROMNamespaceEntry), entry);
  
  // Mark as invalid
  entry.valid = 0;
  EEPROM.put(EEPROM_DIR_OFFSET + ns_idx * sizeof(EEPROMNamespaceEntry), entry);
  EEPROM.commit();
  
  return true;
}

static bool _fs_clean_dir(const char* path) {
  // Clear all namespaces
  for (int i = 0; i < EEPROM_MAX_NAMESPACES; i++) {
    EEPROMNamespaceEntry entry = {0};
    EEPROM.put(EEPROM_DIR_OFFSET + i * sizeof(EEPROMNamespaceEntry), entry);
  }
  EEPROM.commit();
  
  return true;
}

static bool _fs_verify(const char* path, const void* buf, int bufsize) {
  uint8_t tmp[bufsize];
  int read = _fs_read(path, tmp, bufsize);
  
  if (read == bufsize) {
    return memcmp(buf, tmp, bufsize) == 0;
  }
  
  return false;
}
