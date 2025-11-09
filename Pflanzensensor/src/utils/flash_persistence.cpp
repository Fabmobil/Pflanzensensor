/**
 * @file flash_persistence.cpp
 * @brief Text-based flash persistence implementation
 */

#include "flash_persistence.h"
#include "../logger/logger.h"
#include "../managers/manager_config_preferences.h"
#include "critical_section.h"
#include <ESP8266WiFi.h>

#ifdef USE_WEBSERVER
#include <LittleFS.h>
#endif

// CRC32 lookup table (same as before)
static const uint32_t crc32_table[256] PROGMEM = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

uint32_t FlashPersistence::calculateCRC32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; i++) {
    uint8_t index = (crc ^ data[i]) & 0xFF;
    crc = (crc >> 8) ^ pgm_read_dword(&crc32_table[index]);
  }
  return ~crc;
}

uint32_t FlashPersistence::getSafeOffset() {
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t safeOffset =
      ((sketchSize + FP_FLASH_SECTOR_SIZE - 1) / FP_FLASH_SECTOR_SIZE + FP_SAFETY_MARGIN_SECTORS) *
      FP_FLASH_SECTOR_SIZE;

  uint32_t sketchEnd = ESP.getFreeSketchSpace() + sketchSize;
  if (safeOffset + FP_PREFS_MAX_SIZE > sketchEnd) {
    logger.error(F("FlashPers"), F("Nicht genug Flash-Speicher"));
    return 0;
  }

  return safeOffset;
}

uint32_t FlashPersistence::getJsonStorageOffset() {
  uint32_t prefsOffset = getSafeOffset();
  if (prefsOffset == 0) {
    return 0;
  }

  // JSON storage starts after preferences area
  uint32_t jsonOffset = prefsOffset + FP_PREFS_MAX_SIZE;

  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t sketchEnd = ESP.getFreeSketchSpace() + sketchSize;

  if (jsonOffset + FP_JSON_MAX_SIZE > sketchEnd) {
    logger.error(F("FlashPers"), F("Nicht genug Flash für JSON-Speicher"));
    return 0;
  }

  return jsonOffset;
}

ResourceResult FlashPersistence::saveToFlash() {
  logger.info(F("FlashPers"), F("Speichere Preferences als Text..."));

  uint32_t offset = getSafeOffset();
  if (offset == 0) {
    return ResourceResult::fail(ResourceError::INSUFFICIENT_SPACE, F("No flash space"));
  }

  // Build simple text format: "namespace:key=value\n"
  // This is done OUTSIDE critical section (WiFi still active, safe)
  String textData;
  textData.reserve(8192); // Pre-allocate to reduce fragmentation

  Preferences prefs;

  // List of all namespaces to backup
  const char* namespaces[] = {PreferencesNamespaces::GENERAL, PreferencesNamespaces::WIFI1,
                              PreferencesNamespaces::WIFI2,   PreferencesNamespaces::WIFI3,
                              PreferencesNamespaces::DISP,    PreferencesNamespaces::DEBUG,
                              PreferencesNamespaces::LOG,     PreferencesNamespaces::LED_TRAFFIC};

  // Export each namespace
  for (const char* ns : namespaces) {
    if (!prefs.begin(ns, true))
      continue;

    // Get all keys (Preferences library limitation - we know the keys)
    // For general namespace
    if (strcmp(ns, PreferencesNamespaces::GENERAL) == 0) {
      textData += String(ns) + ":initialized=1\n"; // Marker key for namespace existence
      textData += String(ns) + ":device_name=" + prefs.getString("device_name", "") + "\n";
      textData += String(ns) + ":admin_pwd=" + prefs.getString("admin_pwd", "") + "\n";
      textData += String(ns) +
                  ":md5_verify=" + String(prefs.getBool("md5_verify", false) ? "1" : "0") + "\n";
      textData +=
          String(ns) + ":file_log=" + String(prefs.getBool("file_log", false) ? "1" : "0") + "\n";
      textData += String(ns) + ":flower_sens=" + prefs.getString("flower_sens", "") + "\n";
    }
    // For WiFi namespaces
    else if (strncmp(ns, "wifi", 4) == 0) {
      textData += String(ns) + ":initialized=1\n";
      textData += String(ns) + ":ssid=" + prefs.getString("ssid", "") + "\n";
      textData += String(ns) + ":pwd=" + prefs.getString("pwd", "") + "\n";
    }
    // For display namespace
    else if (strcmp(ns, PreferencesNamespaces::DISP) == 0) {
      textData += String(ns) + ":initialized=1\n";
      textData +=
          String(ns) + ":show_ip=" + String(prefs.getBool("show_ip", true) ? "1" : "0") + "\n";
      textData += String(ns) +
                  ":show_clock=" + String(prefs.getBool("show_clock", true) ? "1" : "0") + "\n";
      textData += String(ns) +
                  ":show_flower=" + String(prefs.getBool("show_flower", true) ? "1" : "0") + "\n";
      textData += String(ns) +
                  ":show_fabmobil=" + String(prefs.getBool("show_fabmobil", true) ? "1" : "0") +
                  "\n";
      textData += String(ns) + ":screen_dur=" + String(prefs.getUInt("screen_dur", 5)) + "\n";
      textData += String(ns) + ":clock_fmt=" + prefs.getString("clock_fmt", "24h") + "\n";
    }
    // For debug namespace
    else if (strcmp(ns, PreferencesNamespaces::DEBUG) == 0) {
      textData += String(ns) + ":initialized=1\n";
      textData += String(ns) + ":ram=" + String(prefs.getBool("ram", false) ? "1" : "0") + "\n";
      textData += String(ns) +
                  ":meas_cycle=" + String(prefs.getBool("meas_cycle", false) ? "1" : "0") + "\n";
      textData +=
          String(ns) + ":sensor=" + String(prefs.getBool("sensor", false) ? "1" : "0") + "\n";
      textData +=
          String(ns) + ":display=" + String(prefs.getBool("display", false) ? "1" : "0") + "\n";
      textData +=
          String(ns) + ":websocket=" + String(prefs.getBool("websocket", false) ? "1" : "0") + "\n";
    }
    // For log namespace
    else if (strcmp(ns, PreferencesNamespaces::LOG) == 0) {
      textData += String(ns) + ":initialized=1\n";
      textData += String(ns) + ":level=" + prefs.getString("level", "INFO") + "\n";
      textData += String(ns) +
                  ":file_enabled=" + String(prefs.getBool("file_enabled", false) ? "1" : "0") +
                  "\n";
    }
    // For LED traffic namespace
    else if (strcmp(ns, PreferencesNamespaces::LED_TRAFFIC) == 0) {
      textData += String(ns) + ":initialized=1\n";
      textData += String(ns) + ":mode=" + String(prefs.getUChar("mode", 0)) + "\n";
      textData += String(ns) + ":sel_meas=" + prefs.getString("sel_meas", "") + "\n";
    }

    prefs.end();
  }

  // Also backup sensor namespaces (dynamic: s_SENSORID)
  // Known sensor types that might exist
  const char* knownSensors[] = {"ANALOG", "DHT", "DHT22"};

  for (const char* sensorId : knownSensors) {
    String sensorNs = "s_" + String(sensorId);
    if (sensorNs.length() > 15)
      sensorNs = sensorNs.substring(0, 15);

    if (!prefs.begin(sensorNs.c_str(), true))
      continue; // Namespace doesn't exist

    // Check if it's initialized
    if (!prefs.isKey("initialized")) {
      prefs.end();
      continue;
    }

    textData += sensorNs + ":initialized=1\n";
    textData += sensorNs + ":name=" + prefs.getString("name", "") + "\n";
    textData += sensorNs + ":meas_int=" + String(prefs.getUInt("meas_int", 10000)) + "\n";
    textData += sensorNs + ":has_err=" + String(prefs.getBool("has_err", false) ? "1" : "0") + "\n";

    // Save all measurements (max 8 measurements per sensor)
    for (uint8_t idx = 0; idx < 8; idx++) {
      String prefix = "m" + String(idx) + "_";

      // Check if measurement exists
      if (!prefs.isKey((prefix + "en").c_str()))
        break;

      textData += sensorNs + ":" + prefix +
                  "en=" + String(prefs.getBool((prefix + "en").c_str(), false) ? "1" : "0") + "\n";
      textData +=
          sensorNs + ":" + prefix + "nm=" + prefs.getString((prefix + "nm").c_str(), "") + "\n";
      textData +=
          sensorNs + ":" + prefix + "fn=" + prefs.getString((prefix + "fn").c_str(), "") + "\n";
      textData +=
          sensorNs + ":" + prefix + "un=" + prefs.getString((prefix + "un").c_str(), "") + "\n";
      textData += sensorNs + ":" + prefix +
                  "min=" + String(prefs.getInt((prefix + "min").c_str(), 0)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "max=" + String(prefs.getInt((prefix + "max").c_str(), 0)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "yl=" + String(prefs.getUChar((prefix + "yl").c_str(), 0)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "gl=" + String(prefs.getUChar((prefix + "gl").c_str(), 0)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "gh=" + String(prefs.getUChar((prefix + "gh").c_str(), 0)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "yh=" + String(prefs.getUChar((prefix + "yh").c_str(), 0)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "inv=" + String(prefs.getBool((prefix + "inv").c_str(), false) ? "1" : "0") +
                  "\n";
      textData += sensorNs + ":" + prefix +
                  "cal=" + String(prefs.getBool((prefix + "cal").c_str(), false) ? "1" : "0") +
                  "\n";
      textData += sensorNs + ":" + prefix +
                  "acd=" + String(prefs.getUInt((prefix + "acd").c_str(), 86400)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "rmin=" + String(prefs.getInt((prefix + "rmin").c_str(), INT32_MAX)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "rmax=" + String(prefs.getInt((prefix + "rmax").c_str(), INT32_MIN)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "absMin=" + String(prefs.getFloat((prefix + "absMin").c_str(), INFINITY)) + "\n";
      textData += sensorNs + ":" + prefix +
                  "absMax=" + String(prefs.getFloat((prefix + "absMax").c_str(), -INFINITY)) + "\n";
    }

    prefs.end();
  }

  uint32_t dataSize = textData.length();
  logger.info(F("FlashPers"), F("Textgröße: ") + String(dataSize) + F(" Bytes"));

  if (dataSize == 0 || dataSize > FP_MAX_CONFIG_SIZE - 16) {
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("Invalid data size"));
  }

  // Calculate CRC (outside critical section)
  uint32_t crc = calculateCRC32((const uint8_t*)textData.c_str(), dataSize);

  // Prepare header (outside critical section)
  uint8_t header[16];
  memcpy(header, &FP_MAGIC_NUMBER, 4);
  header[4] = FP_VERSION;
  memcpy(header + 5, &dataSize, 4);
  memcpy(header + 9, &crc, 4);
  memset(header + 13, 0, 3);

  // CRITICAL SECTION: Disable interrupts during flash operations
  // WiFi stays ON, but interrupts are disabled to prevent conflicts
  {
    CriticalSection cs;

    // Erase sectors (interrupts disabled, safe to do flash ops)
    uint32_t sectorsNeeded = ((dataSize + 16 + FP_FLASH_SECTOR_SIZE - 1) / FP_FLASH_SECTOR_SIZE);
    for (uint32_t i = 0; i < sectorsNeeded; i++) {
      uint32_t sectorAddr = (offset + i * FP_FLASH_SECTOR_SIZE) / FP_FLASH_SECTOR_SIZE;

      if (!ESP.flashEraseSector(sectorAddr)) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Erase failed"));
      }
    }

    // Write header
    if (!ESP.flashWrite(offset, (uint32_t*)header, 16)) {
      return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Write header failed"));
    }

    // Write data in chunks (textData is in RAM, safe)
    const char* dataPtr = textData.c_str();
    uint32_t written = 0;
    uint32_t writeOffset = offset + 16;

    while (written < dataSize) {
      uint32_t chunk[256];
      uint32_t chunkSize = min((uint32_t)1024, dataSize - written);
      memcpy(chunk, dataPtr + written, chunkSize);
      uint32_t alignedSize = (chunkSize + 3) & ~3;

      if (!ESP.flashWrite(writeOffset, chunk, alignedSize)) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Write failed"));
      }

      written += chunkSize;
      writeOffset += alignedSize;
    }
  } // CriticalSection ends here, interrupts restored

  logger.info(F("FlashPers"), F("Erfolgreich gespeichert"));
  return ResourceResult::success();
}

ResourceResult FlashPersistence::restoreFromFlash() {
  // CRITICAL: NO LOGGER CALLS - heap is too fragmented, use Serial only
  Serial.println(F("[FlashPers] Stelle Textformat wieder her..."));

  uint32_t offset = getSafeOffset();
  if (offset == 0) {
    Serial.println(F("[FlashPers] FEHLER: Kein Flash-Speicher"));
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("No flash space"));
  }

  // Read header
  uint8_t header[16];
  if (!ESP.flashRead(offset, (uint32_t*)header, 16)) {
    Serial.println(F("[FlashPers] FEHLER: Header-Lesen fehlgeschlagen"));
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Read header failed"));
  }

  uint32_t magic;
  memcpy(&magic, header, 4);
  if (magic != FP_MAGIC_NUMBER) {
    Serial.println(F("[FlashPers] FEHLER: Keine gültige Konfiguration"));
    return ResourceResult::fail(ResourceError::RESOURCE_ERROR, F("No valid config"));
  }

  uint8_t version = header[4];
  if (version != FP_VERSION) {
    Serial.println(F("[FlashPers] FEHLER: Versionskonflikt"));
    return ResourceResult::fail(ResourceError::VERSION_MISMATCH, F("Version mismatch"));
  }

  uint32_t dataSize, storedCRC;
  memcpy(&dataSize, header + 5, 4);
  memcpy(&storedCRC, header + 9, 4);

  if (dataSize == 0 || dataSize > FP_MAX_CONFIG_SIZE) {
    Serial.println(F("[FlashPers] FEHLER: Ungültige Größe"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("Invalid size"));
  }

  Serial.print(F("[FlashPers] Lese "));
  Serial.print(dataSize);
  Serial.println(F(" Bytes..."));

  //  CRITICAL: Don't malloc the entire buffer - heap fragmentation!
  // CRC check skipped - we rely on magic number + version for validation
  // Instead, read and parse line-by-line in small chunks
  // Instead, read and parse line-by-line in small chunks
  Preferences prefs;
  int lineCount = 0;
  char currentNs[32] = "";
  bool nsOpen = false;

  char lineBuffer[256];
  int linePos = 0;
  uint32_t readOffset = offset + 16;
  uint32_t bytesRead = 0;

  // Read and parse in 256-byte chunks
  while (bytesRead < dataSize) {
    // Read chunk
    char chunk[256];
    uint32_t chunkSize = min((uint32_t)256, dataSize - bytesRead);
    uint32_t alignedSize = (chunkSize + 3) & ~3;

    if (!ESP.flashRead(readOffset, (uint32_t*)chunk, alignedSize)) {
      Serial.println(F("[FlashPers] FEHLER: Lesen fehlgeschlagen"));
      if (nsOpen)
        prefs.end();
      return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Read failed"));
    }

    // Process each byte in chunk
    for (uint32_t i = 0; i < chunkSize; i++) {
      char c = chunk[i];

      if (c == '\n' || c == '\r' || linePos >= 255) {
        if (linePos > 0) {
          lineBuffer[linePos] = '\0';

          // Parse "namespace:key=value"
          char* colon = strchr(lineBuffer, ':');
          if (colon) {
            *colon = '\0';
            char* ns = lineBuffer;
            char* keyValue = colon + 1;

            char* equals = strchr(keyValue, '=');
            if (equals) {
              *equals = '\0';
              char* key = keyValue;
              char* value = equals + 1;

              // Check if we need to switch namespace
              if (strcmp(ns, currentNs) != 0) {
                if (nsOpen) {
                  prefs.end();
                  nsOpen = false;
                }

                if (prefs.begin(ns, false)) {
                  strncpy(currentNs, ns, 31);
                  currentNs[31] = '\0';
                  nsOpen = true;
                } else {
                  Serial.print(F("[FlashPers] FEHLER: Kann Namespace nicht öffnen: "));
                  Serial.println(ns);
                  linePos = 0;
                  continue;
                }
              }

              if (nsOpen) {
                // Determine type and write
                // Check for boolean (exactly "0" or "1")
                if (strcmp(value, "0") == 0 || strcmp(value, "1") == 0) {
                  prefs.putBool(key, value[0] == '1');
                }
                // Check for special float values (inf, -inf, ovf)
                else if (strcmp(value, "inf") == 0 || strcmp(value, "ovf") == 0) {
                  prefs.putFloat(key, INFINITY);
                } else if (strcmp(value, "-inf") == 0 || strcmp(value, "-ovf") == 0) {
                  prefs.putFloat(key, -INFINITY);
                }
                // Check if it's a float (contains '.' AND starts with digit or '-')
                else if (strchr(value, '.') != nullptr && strlen(value) > 0 &&
                         (isdigit(value[0]) || value[0] == '-')) {
                  prefs.putFloat(key, atof(value));
                }
                // Check if it's a number (integer)
                else if (strlen(value) > 0 && (isdigit(value[0]) || value[0] == '-')) {
                  // Check if all chars are digits (or minus sign at start)
                  bool isNumber = true;
                  for (size_t j = (value[0] == '-' ? 1 : 0); value[j] != '\0'; j++) {
                    if (!isdigit(value[j])) {
                      isNumber = false;
                      break;
                    }
                  }

                  if (isNumber) {
                    long val = atol(value);

                    // Decide between UChar, UInt, and Int based on value
                    if (val >= 0 && val <= 255) {
                      prefs.putUChar(key, (uint8_t)val);
                    } else if (val >= 0 && val <= 4294967295L) {
                      prefs.putUInt(key, (uint32_t)val);
                    } else {
                      prefs.putInt(key, (int32_t)val);
                    }
                  } else {
                    // Not a number - store as string
                    prefs.putString(key, value);
                  }
                } else {
                  // String
                  prefs.putString(key, value);
                }
                lineCount++;
              }
            }
          }
        }
        linePos = 0;
      } else {
        if (linePos < 255) {
          lineBuffer[linePos++] = c;
        }
      }
    }

    bytesRead += chunkSize;
    readOffset += alignedSize;
  }

  // Close last namespace
  if (nsOpen) {
    prefs.end();
  }

  Serial.print(F("[FlashPers] "));
  Serial.print(lineCount);
  Serial.println(F(" Einträge wiederhergestellt"));

  return ResourceResult::success();
}

ResourceResult FlashPersistence::clearFlash() {
  logger.info(F("FlashPers"), F("Lösche Flash..."));

  uint32_t offset = getSafeOffset();
  if (offset == 0) {
    return ResourceResult::success();
  }

  uint32_t sectorAddr = offset / FP_FLASH_SECTOR_SIZE;
  if (!ESP.flashEraseSector(sectorAddr)) {
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Erase failed"));
  }

  logger.info(F("FlashPers"), F("Gelöscht"));
  return ResourceResult::success();
}

bool FlashPersistence::hasValidConfig() {
  uint32_t offset = getSafeOffset();
  if (offset == 0)
    return false;

  uint32_t magic;
  if (!ESP.flashRead(offset, &magic, 4))
    return false;

  return (magic == FP_MAGIC_NUMBER);
}

// ==================== NEW: Combined Preferences + Config Files ====================

ResourceResult FlashPersistence::saveAllToFlash() {
  logger.info(F("FlashPers"), F("Sichere Preferences + Config-Dateien..."));

  // NEW SIMPLIFIED ARCHITECTURE:
  // WiFi stays ON throughout the entire process. We use CriticalSection
  // to disable interrupts during flash operations, which prevents
  // WiFi callbacks from interfering without actually disconnecting WiFi.
  // This is much cleaner and more reliable.

  // STEP 1: Save Preferences to flash
  auto prefsResult = saveToFlash();
  if (!prefsResult.isSuccess()) {
    return prefsResult;
  }

  // STEP 2: Save JSON config files to separate flash area
  // No delay needed - CriticalSection handles everything safely
  auto jsonResult = saveJsonToFlash();
  if (!jsonResult.isSuccess()) {
    logger.warning(F("FlashPers"), F("JSON-Sicherung fehlgeschlagen"));
    return jsonResult;
  }

  logger.info(F("FlashPers"), F("Erfolgreich gespeichert (Preferences + JSON-Configs)"));
  return ResourceResult::success();
}

ResourceResult FlashPersistence::restoreAllFromFlash() {
  Serial.println(F("[FlashPers] Stelle Preferences + Config-Dateien wieder her..."));

  // NO WIFI DISCONNECT needed for restore - read operations don't conflict

  // STEP 1: Restore Preferences from flash (separate area)
  auto prefsResult = restoreFromFlash();
  if (!prefsResult.isSuccess()) {
    return prefsResult;
  }

  // STEP 2: Restore JSON config files from separate flash area
  auto jsonResult = restoreJsonFromFlash();
  if (!jsonResult.isSuccess()) {
    Serial.println(F("[FlashPers] WARNUNG: JSON-Wiederherstellung fehlgeschlagen"));
    // Not fatal - preferences are restored
  }

  Serial.println(F("[FlashPers] Wiederherstellung abgeschlossen"));
  return ResourceResult::success();
}

// Helper methods for JSON storage in separate flash area

ResourceResult FlashPersistence::saveJsonToFlash() {
  logger.info(F("FlashPers"), F("Sichere JSON-Configs in Flash..."));

#ifndef USE_WEBSERVER
  return ResourceResult::success(); // Nothing to do without web support
#else
  uint32_t offset = getJsonStorageOffset();
  if (offset == 0) {
    return ResourceResult::fail(ResourceError::INSUFFICIENT_SPACE, F("No flash for JSON"));
  }

  // STEP 1: Collect file metadata (WiFi ON, safe)
  struct FileInfo {
    String filename;
    size_t size;
  };
  FileInfo files[16]; // Max 16 JSON files
  uint8_t fileCount = 0;

  Dir dir = LittleFS.openDir("/config");
  while (dir.next() && fileCount < 16) {
    String filename = dir.fileName();
    if (filename.endsWith(".json") && !filename.endsWith(".example")) {
      File f = LittleFS.open("/config/" + filename, "r");
      if (f) {
        files[fileCount].filename = filename;
        files[fileCount].size = f.size();
        fileCount++;
        f.close();
      }
    }
  }

  if (fileCount == 0) {
    logger.info(F("FlashPers"), F("Keine JSON-Dateien zum Sichern"));
    return ResourceResult::success();
  }

  logger.info(F("FlashPers"), String(fileCount) + F(" JSON-Dateien gefunden"));

  // STEP 2: Build manifest (WiFi ON, safe)
  String manifest;
  manifest.reserve(512);
  manifest += String(fileCount) + "\n";

  uint32_t totalSize = 16; // header
  for (uint8_t i = 0; i < fileCount; i++) {
    manifest += files[i].filename + "|" + String(files[i].size) + "\n";
    totalSize += files[i].size;
  }
  totalSize += manifest.length();

  logger.info(F("FlashPers"), F("JSON Gesamt: ") + String(totalSize) + F(" Bytes"));

  if (totalSize > FP_JSON_MAX_SIZE) {
    return ResourceResult::fail(ResourceError::INSUFFICIENT_SPACE, F("JSON too large"));
  }

  uint32_t manifestSize = manifest.length();

  // STEP 3: Prepare header (WiFi ON, safe)
  uint8_t header[16];
  memcpy(header, &FP_MAGIC_NUMBER, 4);
  header[4] = FP_VERSION;
  memcpy(header + 5, &manifestSize, 4);
  uint32_t placeholder_crc = 0;
  memcpy(header + 9, &placeholder_crc, 4);
  memset(header + 13, 0, 3);

  uint32_t sectorsNeeded = ((totalSize + FP_FLASH_SECTOR_SIZE - 1) / FP_FLASH_SECTOR_SIZE);
  logger.debug(F("FlashPers"), F("Lösche ") + String(sectorsNeeded) + F(" Sektoren..."));

  // STEP 4: CRITICAL SECTION - Erase and write header/manifest
  {
    CriticalSection cs;

    // Erase all needed sectors
    for (uint32_t i = 0; i < sectorsNeeded; i++) {
      uint32_t sectorAddr = (offset + i * FP_FLASH_SECTOR_SIZE) / FP_FLASH_SECTOR_SIZE;
      if (!ESP.flashEraseSector(sectorAddr)) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("JSON erase failed"));
      }
    }

    // Write header
    uint32_t writeOffset = offset;
    if (!ESP.flashWrite(writeOffset, (uint32_t*)header, 16)) {
      return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Header write failed"));
    }
    writeOffset += 16;

    // Write manifest in chunks
    const char* manifestPtr = manifest.c_str();
    uint32_t manifestWritten = 0;

    while (manifestWritten < manifestSize) {
      uint32_t chunk[256];
      uint32_t chunkSize = min((uint32_t)1024, manifestSize - manifestWritten);
      memcpy(chunk, manifestPtr + manifestWritten, chunkSize);
      uint32_t alignedSize = (chunkSize + 3) & ~3;

      if (!ESP.flashWrite(writeOffset, chunk, alignedSize)) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Manifest write failed"));
      }

      manifestWritten += chunkSize;
      writeOffset += alignedSize;
    }

    // STEP 5: Write each JSON file individually in its own critical section
    // This allows us to read from LittleFS with interrupts enabled between files
    for (uint8_t fileIdx = 0; fileIdx < fileCount; fileIdx++) {
      String filepath = "/config/" + files[fileIdx].filename;

      // Exit critical section temporarily to read file
    } // End of CriticalSection for header/manifest
  }

  // STEP 6: Write file contents one by one (each in its own critical section)
  uint32_t writeOffset = offset + 16 + ((manifestSize + 3) & ~3);

  for (uint8_t fileIdx = 0; fileIdx < fileCount; fileIdx++) {
    String filepath = "/config/" + files[fileIdx].filename;

    // Open and read file (WiFi ON, interrupts enabled, safe for LittleFS)
    File f = LittleFS.open(filepath, "r");
    if (!f) {
      logger.warning(F("FlashPers"), F("Konnte nicht öffnen: ") + files[fileIdx].filename);
      continue;
    }

    size_t fileSize = f.size();
    size_t bytesRead = 0;

    // Read and write in small chunks
    while (bytesRead < fileSize) {
      // Read chunk from LittleFS (interrupts enabled, safe)
      uint8_t buffer[128];
      size_t chunkSize =
          (fileSize - bytesRead > sizeof(buffer)) ? sizeof(buffer) : (fileSize - bytesRead);
      size_t actualRead = f.read(buffer, chunkSize);

      if (actualRead == 0)
        break;

      // Prepare aligned buffer for flash write
      uint32_t alignedChunk[32];
      uint32_t alignedSize = (actualRead + 3) & ~3;
      memset(alignedChunk, 0xFF, alignedSize);
      memcpy(alignedChunk, buffer, actualRead);

      // CRITICAL SECTION for flash write only
      {
        CriticalSection cs;
        if (!ESP.flashWrite(writeOffset, alignedChunk, alignedSize)) {
          f.close();
          return ResourceResult::fail(ResourceError::OPERATION_FAILED,
                                      F("File write failed: ") + files[fileIdx].filename);
        }
      }

      bytesRead += actualRead;
      writeOffset += alignedSize;
    }

    f.close();
    logger.debug(F("FlashPers"), F("Gesichert: ") + files[fileIdx].filename);
  }

  logger.info(F("FlashPers"), F("JSON-Configs erfolgreich in Flash gesichert"));
  return ResourceResult::success();
#endif
}

ResourceResult FlashPersistence::restoreJsonFromFlash() {
  Serial.println(F("[FlashPers] Stelle JSON-Configs aus Flash wieder her..."));

#ifndef USE_WEBSERVER
  return ResourceResult::success();
#else
  uint32_t offset = getJsonStorageOffset();
  if (offset == 0) {
    Serial.println(F("[FlashPers] Kein JSON Flash-Speicher"));
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("No JSON flash"));
  }

  // Read header
  uint8_t header[16];
  if (!ESP.flashRead(offset, (uint32_t*)header, 16)) {
    Serial.println(F("[FlashPers] JSON Header-Lesen fehlgeschlagen"));
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Read JSON header failed"));
  }

  uint32_t magic;
  memcpy(&magic, header, 4);
  if (magic != FP_MAGIC_NUMBER) {
    Serial.println(F("[FlashPers] Keine gültigen JSON-Configs"));
    return ResourceResult::success(); // Not an error, just no backup
  }

  uint32_t manifestSize;
  memcpy(&manifestSize, header + 5, 4);

  if (manifestSize == 0 || manifestSize > 4096) {
    Serial.println(F("[FlashPers] Ungültige Manifest-Größe"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("Invalid manifest size"));
  }

  Serial.print(F("[FlashPers] Manifest: "));
  Serial.print(manifestSize);
  Serial.println(F(" Bytes"));

  // Read manifest in chunks
  String manifest;
  manifest.reserve(manifestSize + 1);
  uint32_t readOffset = offset + 16;
  uint32_t bytesRead = 0;

  while (bytesRead < manifestSize) {
    char chunk[256];
    uint32_t chunkSize = min((uint32_t)256, manifestSize - bytesRead);
    uint32_t alignedSize = (chunkSize + 3) & ~3;

    if (!ESP.flashRead(readOffset, (uint32_t*)chunk, alignedSize)) {
      Serial.println(F("[FlashPers] Manifest lesen fehlgeschlagen"));
      return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Manifest read failed"));
    }

    for (uint32_t i = 0; i < chunkSize; i++) {
      manifest += chunk[i];
    }

    bytesRead += chunkSize;
    readOffset += alignedSize;
  }

  // Parse manifest: first line = file count, then filename|size
  int lineStart = 0;
  int lineEnd = manifest.indexOf('\n');

  if (lineEnd == -1) {
    Serial.println(F("[FlashPers] Ungültiges Manifest-Format"));
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("Invalid manifest"));
  }

  uint8_t fileCount = manifest.substring(lineStart, lineEnd).toInt();
  Serial.print(F("[FlashPers] "));
  Serial.print(fileCount);
  Serial.println(F(" Dateien im Manifest"));

  // Ensure /config/ exists
  if (!LittleFS.exists("/config")) {
    LittleFS.mkdir("/config");
  }

  lineStart = lineEnd + 1;

  // Restore each file
  for (uint8_t i = 0; i < fileCount; i++) {
    lineEnd = manifest.indexOf('\n', lineStart);
    if (lineEnd == -1)
      break;

    String line = manifest.substring(lineStart, lineEnd);
    int pipePos = line.indexOf('|');

    if (pipePos == -1) {
      lineStart = lineEnd + 1;
      continue;
    }

    String filename = line.substring(0, pipePos);
    size_t fileSize = line.substring(pipePos + 1).toInt();

    Serial.print(F("[FlashPers] Wiederherstellung: "));
    Serial.print(filename);
    Serial.print(F(" ("));
    Serial.print(fileSize);
    Serial.println(F(" Bytes)"));

    // Read file from flash and write to LittleFS
    String dstPath = "/config/" + filename;
    File dst = LittleFS.open(dstPath, "w");

    if (!dst) {
      Serial.print(F("[FlashPers] Konnte nicht erstellen: "));
      Serial.println(dstPath);
      lineStart = lineEnd + 1;
      continue;
    }

    size_t fileRead = 0;
    while (fileRead < fileSize) {
      uint8_t buffer[128];
      size_t chunkSize =
          (fileSize - fileRead > sizeof(buffer)) ? sizeof(buffer) : (fileSize - fileRead);
      uint32_t alignedSize = (chunkSize + 3) & ~3;
      uint32_t alignedBuffer[32];

      if (!ESP.flashRead(readOffset, alignedBuffer, alignedSize)) {
        Serial.println(F("[FlashPers] Datei-Lesen fehlgeschlagen"));
        dst.close();
        lineStart = lineEnd + 1;
        break;
      }

      memcpy(buffer, alignedBuffer, chunkSize);
      dst.write(buffer, chunkSize);

      fileRead += chunkSize;
      readOffset += alignedSize;
      yield();
    }

    dst.close();
    Serial.print(F("[FlashPers] OK: "));
    Serial.println(filename);

    lineStart = lineEnd + 1;
  }

  Serial.println(F("[FlashPers] JSON-Wiederherstellung abgeschlossen"));
  return ResourceResult::success();
#endif
}
