/**
 * @file PreferencesEEPROM.cpp
 * @brief Implementation of EEPROM-based Preferences replacement
 */

#include "PreferencesEEPROM.h"
#include "../logger/logger.h"

bool PreferencesEEPROM::_initialized = false;

PreferencesEEPROM::PreferencesEEPROM() : _readOnly(false), _namespaceIndex(-1), _dataOffset(0) {
  _namespace[0] = '\0';
}

PreferencesEEPROM::~PreferencesEEPROM() { end(); }

bool PreferencesEEPROM::initializeStorage() {
  if (_initialized) {
    return true;
  }

  EEPROM.begin(PREFS_EEPROM_SIZE);

  // Check if EEPROM is already initialized
  uint16_t magic = 0;
  EEPROM.get(EEPROM_HEADER_OFFSET, magic);

  if (magic != PREFS_MAGIC) {
    // Initialize EEPROM header
    EEPROM.put(EEPROM_HEADER_OFFSET, PREFS_MAGIC);
    uint8_t version = PREFS_VERSION;
    EEPROM.put(EEPROM_HEADER_OFFSET + 2, version);

    // Clear namespace directory
    for (int i = 0; i < MAX_NAMESPACES; i++) {
      NamespaceEntry entry = {0};
      EEPROM.put(EEPROM_DIR_OFFSET + i * sizeof(NamespaceEntry), entry);
    }

    EEPROM.commit();
    logger.info(F("PrefsEEPROM"), F("EEPROM storage initialized"));
  }

  _initialized = true;
  return true;
}

int PreferencesEEPROM::findNamespace(const char* name) {
  for (int i = 0; i < MAX_NAMESPACES; i++) {
    NamespaceEntry entry;
    EEPROM.get(EEPROM_DIR_OFFSET + i * sizeof(NamespaceEntry), entry);

    if (entry.initialized && strcmp(entry.name, name) == 0) {
      return i;
    }
  }
  return -1;
}

int PreferencesEEPROM::createNamespace(const char* name) {
  // Find free slot
  for (int i = 0; i < MAX_NAMESPACES; i++) {
    NamespaceEntry entry;
    EEPROM.get(EEPROM_DIR_OFFSET + i * sizeof(NamespaceEntry), entry);

    if (!entry.initialized) {
      // Found free slot
      memset(&entry, 0, sizeof(entry));
      strncpy(entry.name, name, NAMESPACE_NAME_LENGTH);
      entry.offset = EEPROM_DATA_OFFSET + (i * NAMESPACE_DATA_SIZE);
      entry.size = NAMESPACE_DATA_SIZE;
      entry.initialized = 1;

      EEPROM.put(EEPROM_DIR_OFFSET + i * sizeof(NamespaceEntry), entry);
      EEPROM.commit();

      logger.info(F("PrefsEEPROM"), String(F("Created namespace: ")) + name);
      return i;
    }
  }

  logger.error(F("PrefsEEPROM"), F("No free namespace slots"));
  return -1;
}

bool PreferencesEEPROM::begin(const char* name, bool readOnly) {
  if (!_initialized) {
    initializeStorage();
  }

  // Close previous namespace if open
  if (_namespaceIndex >= 0) {
    end();
  }

  _readOnly = readOnly;
  strncpy(_namespace, name, NAMESPACE_NAME_LENGTH);
  _namespace[NAMESPACE_NAME_LENGTH] = '\0';

  // Find existing namespace
  _namespaceIndex = findNamespace(name);

  if (_namespaceIndex < 0) {
    if (readOnly) {
      // Can't create in read-only mode
      return false;
    }

    // Create new namespace
    _namespaceIndex = createNamespace(name);
    if (_namespaceIndex < 0) {
      return false;
    }
  }

  // Get namespace data offset
  NamespaceEntry entry;
  EEPROM.get(EEPROM_DIR_OFFSET + _namespaceIndex * sizeof(NamespaceEntry), entry);
  _dataOffset = entry.offset;

  return true;
}

void PreferencesEEPROM::end() {
  if (_namespaceIndex >= 0) {
    _namespaceIndex = -1;
    _dataOffset = 0;
    _namespace[0] = '\0';
  }
}

bool PreferencesEEPROM::clear() {
  if (_namespaceIndex < 0 || _readOnly) {
    return false;
  }

  // Clear namespace data
  for (uint16_t i = 0; i < NAMESPACE_DATA_SIZE; i++) {
    EEPROM.write(_dataOffset + i, 0);
  }

  EEPROM.commit();
  return true;
}

bool PreferencesEEPROM::remove(const char* key) {
  if (_namespaceIndex < 0 || _readOnly) {
    return false;
  }

  uint16_t keyHash = makeKey(key);
  uint8_t marker = 0; // 0 = deleted

  EEPROM.write(_dataOffset + (keyHash % NAMESPACE_DATA_SIZE), marker);
  EEPROM.commit();

  return true;
}

uint16_t PreferencesEEPROM::makeKey(const char* key) {
  // Simple hash function
  uint16_t hash = 0;
  while (*key) {
    hash = hash * 31 + *key++;
  }
  return hash;
}

bool PreferencesEEPROM::writeValue(const char* key, const void* value, size_t len) {
  if (_namespaceIndex < 0 || _readOnly) {
    logger.error(F("PrefsEEPROM"), String(F("writeValue failed: ns=")) + _namespaceIndex + F(" readOnly=") + _readOnly);
    return false;
  }
  uint16_t keyHash = makeKey(key);
  const uint16_t slots = (NAMESPACE_DATA_SIZE / 8);
  uint16_t startSlot = keyHash % slots;
  // New slot layout to avoid collisions:
  // [marker(1)][hash_low(1)][hash_high(1)][payload(5)] = 8 bytes
  // We store the keyHash in every occupied slot so reads can verify matching
  // entries. To avoid overwriting other keys we first allocate a contiguous
  // range of slots that are either free or already owned by this key.

  const uint8_t* src = (const uint8_t*)value;
  size_t remaining = len;
  // calculate how many slots are needed (5 bytes payload per slot)
  size_t neededSlots = (len + 4) / 5; // ceil(len/5)

  logger.debug(F("PrefsEEPROM"), String(F("writeValue: ns=")) + _namespace + 
               F(" key=") + key + F(" len=") + len + F(" needSlots=") + neededSlots + F(" totalSlots=") + slots);

  // Search for contiguous region of neededSlots where each slot is free or matches keyHash
  bool allocated = false;
  uint16_t allocStart = 0;
  for (uint16_t probeStart = 0; probeStart < slots; ++probeStart) {
    // Try starting at (startSlot + probeStart) % slots
    uint16_t s = (startSlot + probeStart) % slots;
    bool ok = true;
    for (size_t k = 0; k < neededSlots; ++k) {
      uint16_t slotIndex = (s + k) % slots;
      uint16_t offset = _dataOffset + (slotIndex * 8);
      uint8_t marker = EEPROM.read(offset);
      if (marker == 1) {
        uint16_t storedHash =
            (uint16_t)EEPROM.read(offset + 1) | ((uint16_t)EEPROM.read(offset + 2) << 8);
        if (storedHash != keyHash) {
          ok = false;
          break;
        }
      }
      // marker != 1 is considered free (0 or 255)
    }
    if (ok) {
      allocated = true;
      allocStart = s;
      break;
    }
  }

  if (!allocated) {
    // No contiguous region found
    logger.error(F("PrefsEEPROM"), String(F("writeValue FAILED - no contiguous space: ns=")) + 
                 _namespace + F(" key=") + key + F(" needSlots=") + neededSlots);
    return false;
  }

  logger.debug(F("PrefsEEPROM"), String(F("writeValue allocated at slot ")) + allocStart);

  // Now write into the allocated contiguous region
  uint16_t slotIndex = allocStart;
  while (remaining > 0) {
    uint16_t offset = _dataOffset + (slotIndex * 8);
    // Write marker (1 = valid data)
    EEPROM.write(offset, 1);
    // Store keyHash (little endian) in bytes [offset+1, offset+2]
    EEPROM.write(offset + 1, (uint8_t)(keyHash & 0xFF));
    EEPROM.write(offset + 2, (uint8_t)((keyHash >> 8) & 0xFF));

    size_t writeLen = std::min<size_t>(remaining, 5);
    for (size_t i = 0; i < writeLen; i++) {
      EEPROM.write(offset + 3 + i, src[i]);
    }
    // Zero out remaining payload bytes in the slot to avoid leaking old data
    for (size_t i = writeLen; i < 5; i++) {
      EEPROM.write(offset + 3 + i, 0);
    }

    remaining -= writeLen;
    src += writeLen;
    slotIndex = (slotIndex + 1) % slots;
  }

  EEPROM.commit();
  return true;
}

bool PreferencesEEPROM::readValue(const char* key, void* value, size_t len) {
  if (_namespaceIndex < 0) {
    return false;
  }
  uint16_t keyHash = makeKey(key);
  const uint16_t slots = (NAMESPACE_DATA_SIZE / 8);
  uint16_t startSlot = keyHash % slots;

  uint8_t* dst = (uint8_t*)value;
  size_t remaining = len;
  uint16_t slotIndex = startSlot;
  bool foundAny = false;

  // Linear probe to find the first slot that matches our keyHash
  for (uint16_t probe = 0; probe < slots; ++probe) {
    uint16_t offset = _dataOffset + (slotIndex * 8);
    uint8_t marker = EEPROM.read(offset);
    if (marker != 1) {
      // Empty slot: continue probing
      slotIndex = (slotIndex + 1) % slots;
      continue;
    }
    uint16_t storedHash =
        (uint16_t)EEPROM.read(offset + 1) | ((uint16_t)EEPROM.read(offset + 2) << 8);
    if (storedHash != keyHash) {
      // Not our key, keep probing
      slotIndex = (slotIndex + 1) % slots;
      continue;
    }

    // Found the first slot for our key - now read consecutive slots with matching keyHash
    while (remaining > 0) {
      uint16_t off = _dataOffset + (slotIndex * 8);
      uint8_t mk = EEPROM.read(off);
      if (mk != 1) {
        // Unexpected end
        return foundAny;
      }
      uint16_t sh = (uint16_t)EEPROM.read(off + 1) | ((uint16_t)EEPROM.read(off + 2) << 8);
      if (sh != keyHash) {
        // End of our chained value
        return foundAny;
      }
      size_t readLen = std::min<size_t>(remaining, 5);
      for (size_t i = 0; i < readLen; i++) {
        dst[i] = EEPROM.read(off + 3 + i);
      }
      foundAny = true;
      remaining -= readLen;
      dst += readLen;
      slotIndex = (slotIndex + 1) % slots;
    }
    break;
  }

  return foundAny;
}

bool PreferencesEEPROM::isKey(const char* key) {
  if (_namespaceIndex < 0) {
    return false;
  }

  uint16_t keyHash = makeKey(key);
  uint16_t slot = keyHash % (NAMESPACE_DATA_SIZE / 8);
  uint16_t offset = _dataOffset + (slot * 8);

  return EEPROM.read(offset) == 1;
}

// Getter implementations
String PreferencesEEPROM::getString(const char* key, const String& defaultValue) {
  char buf[MAX_STRING_LENGTH + 1] = {0};
  if (readValue(key, buf, MAX_STRING_LENGTH)) {
    return String(buf);
  }
  return defaultValue;
}

bool PreferencesEEPROM::getBool(const char* key, bool defaultValue) {
  uint8_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value != 0;
  }
  return defaultValue;
}

uint8_t PreferencesEEPROM::getUChar(const char* key, uint8_t defaultValue) {
  uint8_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

uint16_t PreferencesEEPROM::getUShort(const char* key, uint16_t defaultValue) {
  uint16_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

uint32_t PreferencesEEPROM::getUInt(const char* key, uint32_t defaultValue) {
  uint32_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

int8_t PreferencesEEPROM::getChar(const char* key, int8_t defaultValue) {
  int8_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

int16_t PreferencesEEPROM::getShort(const char* key, int16_t defaultValue) {
  int16_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

int32_t PreferencesEEPROM::getInt(const char* key, int32_t defaultValue) {
  int32_t value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

float PreferencesEEPROM::getFloat(const char* key, float defaultValue) {
  float value;
  if (readValue(key, &value, sizeof(value))) {
    return value;
  }
  return defaultValue;
}

// Setter implementations
size_t PreferencesEEPROM::putString(const char* key, const String& value) {
  char buf[MAX_STRING_LENGTH + 1] = {0};
  strncpy(buf, value.c_str(), MAX_STRING_LENGTH);
  return writeValue(key, buf, strlen(buf) + 1) ? strlen(buf) + 1 : 0;
}

size_t PreferencesEEPROM::putBool(const char* key, bool value) {
  uint8_t v = value ? 1 : 0;
  return writeValue(key, &v, sizeof(v)) ? sizeof(v) : 0;
}

size_t PreferencesEEPROM::putUChar(const char* key, uint8_t value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}

size_t PreferencesEEPROM::putUShort(const char* key, uint16_t value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}

size_t PreferencesEEPROM::putUInt(const char* key, uint32_t value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}

size_t PreferencesEEPROM::putChar(const char* key, int8_t value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}

size_t PreferencesEEPROM::putShort(const char* key, int16_t value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}

size_t PreferencesEEPROM::putInt(const char* key, int32_t value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}

size_t PreferencesEEPROM::putFloat(const char* key, float value) {
  return writeValue(key, &value, sizeof(value)) ? sizeof(value) : 0;
}
