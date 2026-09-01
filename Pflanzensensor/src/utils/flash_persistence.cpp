/**
 * @file flash_persistence.cpp
 * @brief Text-based flash persistence implementation
 */

#include "flash_persistence.h"
#include "../logger/logger.h"
#include "../managers/manager_config_preferences.h"
#include "crc32.h"
#include "critical_section.h"
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#if USE_WEBSERVER
#include <LittleFS.h>
#endif

bool FlashPersistence::isChecksumVerificationEnabled() {
  Preferences prefs;
  if (!prefs.begin(PreferencesNamespaces::GENERAL, true)) {
    return false; // Keine Konfiguration vorhanden - nichts zu prüfen
  }
  bool enabled = prefs.getBool("md5_verify", false);
  prefs.end();
  return enabled;
}

uint32_t FlashPersistence::getSafeOffset() {
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t safeOffset =
      ((sketchSize + FP_FLASH_SECTOR_SIZE - 1) / FP_FLASH_SECTOR_SIZE + FP_SAFETY_MARGIN_SECTORS) *
      FP_FLASH_SECTOR_SIZE;

  uint32_t sketchEnd = ESP.getFreeSketchSpace() + sketchSize;
  if (safeOffset + FP_PREFS_MAX_SIZE > sketchEnd) {
    LOG_ERROR(F("FlashPers"), F("Nicht genug Flash-Speicher"));
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
    LOG_ERROR(F("FlashPers"), F("Nicht genug Flash für JSON-Speicher"));
    return 0;
  }

  return jsonOffset;
}

ResourceResult FlashPersistence::saveToFlash() {
  LOG_INFO(F("FlashPers"), F("Speichere Preferences als Text..."));

  uint32_t offset = getSafeOffset();
  if (offset == 0) {
    return ResourceResult::fail(ResourceError::INSUFFICIENT_SPACE, F("No flash space"));
  }

  // Build simple text format: "namespace:key=value\n"
  // This is done OUTSIDE critical section (WiFi still active, safe)
  String textData;
  textData.reserve(8192); // Pre-allocate to reduce fragmentation

  // Jede Zeile stückweise anhängen statt als eine Verkettungskette.
  //
  // "a + b + c + d" hält alle Zwischenergebnisse gleichzeitig im Stapelrahmen;
  // bei rund fünfzig solchen Zeilen in einer Funktion reicht der 4 KB große
  // Loop-Stack des ESP8266 nicht mehr. Das Ergebnis war ein
  // "Panic core_esp8266_main.cpp:215 loop_task" mitten im Sichern - die
  // Sicherung wurde nie fertig, und nach einem Dateisystem-Update kam ein
  // veralteter Stand zurück, ohne dass es jemandem auffiel.
  auto zeile = [&textData](const char* ns, const char* key, const String& wert) {
    textData += ns;
    textData += ':';
    textData += key;
    textData += '=';
    textData += wert;
    textData += '\n';
  };
  auto zeileBool = [&zeile](const char* ns, const char* key, bool wert) {
    zeile(ns, key, wert ? F("1") : F("0"));
  };
  auto zeileZahl = [&zeile](const char* ns, const char* key, uint32_t wert) {
    zeile(ns, key, String(wert));
  };

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
      zeile(ns, "initialized", F("1")); // Marker key for namespace existence
      // Vorgaben identisch zum Ladepfad halten - ein leerer Rückfallwert würde
      // beim Wiederherstellen nach einem Dateisystem-Update ein leeres
      // Adminpasswort setzen, falls der Schlüssel nie explizit geschrieben wurde.
      zeile(ns, "device_name", prefs.getString("device_name", DEVICE_NAME));
      zeile(ns, "admin_pwd", prefs.getString("admin_pwd", ADMIN_PASSWORD));
      zeileBool(ns, "md5_verify", prefs.getBool("md5_verify", false));
      zeileBool(ns, "file_log", prefs.getBool("file_log", FILE_LOGGING_ENABLED));
      zeile(ns, "flower_sens", prefs.getString("flower_sens", ""));
      // Mailversand. Ohne diese Zeilen war die komplette Mailkonfiguration nach
      // jedem Dateisystem-Update weg - Geraete-ID, Schluessel und Empfaenger
      // mussten jedes Mal neu eingetippt werden, und zwar unbemerkt: die
      // Oberflaeche zeigte danach die einkompilierten Vorgaben, nicht die
      // eigenen Werte. Die Liste hier wird von Hand gepflegt (die
      // Preferences-Bibliothek kann ihre Schluessel nicht aufzaehlen), sie muss
      // also zum Ladepfad in manager_config_persistence.cpp passen.
      zeileBool(ns, "mail_on", prefs.getBool("mail_on", MAIL_ENABLED));
      zeile(ns, "mail_url", prefs.getString("mail_url", MAIL_SERVICE_URL));
      zeile(ns, "mail_devid", prefs.getString("mail_devid", MAIL_DEVICE_ID));
      zeile(ns, "mail_key", prefs.getString("mail_key", MAIL_SECRET_KEY));
      zeile(ns, "mail_to", prefs.getString("mail_to", MAIL_TO));
      zeile(ns, "mail_sens", prefs.getString("mail_sens", ""));
      zeileZahl(ns, "mail_warn_h", prefs.getUInt("mail_warn_h", MAIL_WARN_INTERVAL_HOURS));
      zeileZahl(ns, "mail_warn_ab", prefs.getUInt("mail_warn_ab", MAIL_WARN_FROM));
      zeileBool(ns, "mail_boot", prefs.getBool("mail_boot", MAIL_BOOT_ENABLED));
      zeileBool(ns, "mail_alive", prefs.getBool("mail_alive", MAIL_ALIVE_ENABLED));
      zeileZahl(ns, "mail_alive_h", prefs.getUInt("mail_alive_h", MAIL_ALIVE_INTERVAL_HOURS));
    }
    // For WiFi namespaces
    else if (strncmp(ns, "wifi", 4) == 0) {
      zeile(ns, "initialized", F("1"));
      zeile(ns, "ssid", prefs.getString("ssid", ""));
      zeile(ns, "pwd", prefs.getString("pwd", ""));
    }
    // For display namespace
    else if (strcmp(ns, PreferencesNamespaces::DISP) == 0) {
      zeile(ns, "initialized", F("1"));
      textData +=
          String(ns) + ":show_ip=" + String(prefs.getBool("show_ip", true) ? "1" : "0") + "\n";
      zeileBool(ns, "show_clock", prefs.getBool("show_clock", true));
      zeileBool(ns, "show_flower", prefs.getBool("show_flower", true));
      zeileBool(ns, "show_fabmobil", prefs.getBool("show_fabmobil", true));
      zeileZahl(ns, "screen_dur", prefs.getUInt("screen_dur", 5));
      zeile(ns, "clock_fmt", prefs.getString("clock_fmt", "24h"));
    }
    // For debug namespace
    else if (strcmp(ns, PreferencesNamespaces::DEBUG) == 0) {
      zeile(ns, "initialized", F("1"));
      zeileBool(ns, "ram", prefs.getBool("ram", false));
      zeileBool(ns, "meas_cycle", prefs.getBool("meas_cycle", false));
      textData +=
          String(ns) + ":sensor=" + String(prefs.getBool("sensor", false) ? "1" : "0") + "\n";
      textData +=
          String(ns) + ":display=" + String(prefs.getBool("display", false) ? "1" : "0") + "\n";
      textData +=
          String(ns) + ":websocket=" + String(prefs.getBool("websocket", false) ? "1" : "0") + "\n";
    }
    // For log namespace
    else if (strcmp(ns, PreferencesNamespaces::LOG) == 0) {
      zeile(ns, "initialized", F("1"));
      zeile(ns, "level", prefs.getString("level", "INFO"));
      zeileBool(ns, "file_enabled", prefs.getBool("file_enabled", false));
    }
    // For LED traffic namespace
    else if (strcmp(ns, PreferencesNamespaces::LED_TRAFFIC) == 0) {
      zeile(ns, "initialized", F("1"));
      zeileZahl(ns, "mode", prefs.getUChar("mode", 0));
      zeile(ns, "sel_meas", prefs.getString("sel_meas", ""));
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
  LOG_INFO(F("FlashPers"), String(F("Textgröße: ")) + String(dataSize) + F(" Bytes"));

  if (dataSize == 0 || dataSize > FP_MAX_CONFIG_SIZE - 16) {
    return ResourceResult::fail(ResourceError::VALIDATION_ERROR, F("Invalid data size"));
  }

  // Calculate CRC (outside critical section)
  uint32_t crc = Crc32::calculate((const uint8_t*)textData.c_str(), dataSize);

  // Prepare header (outside critical section)
  uint8_t header[16];
  memcpy(header, &FP_MAGIC_NUMBER, 4);
  header[4] = FP_VERSION;
  memcpy(header + 5, &dataSize, 4);
  memcpy(header + 9, &crc, 4);
  memset(header + 13, 0, 3);

  // Flash-Operationen: Interrupts werden NUR um den einzelnen Erase- bzw.
  // Write-Aufruf gesperrt, nicht um die ganze Schleife. Ein Sektor-Erase dauert
  // 20-40 ms; bleiben die Interrupts über alle Sektoren hinweg gesperrt, kann
  // weder der Watchdog gefüttert noch der SDK-Timer bedient werden.
  //
  // Hier KEIN yield()/delay() verwenden: dieser Pfad wird auch aus dem
  // OTA-/Webserver-Kontext heraus aufgerufen, in dem der ESP8266-Core bei
  // yield() mit "Panic core_esp8266_main.cpp __yield" abbricht.
  // ESP.wdtFeed() ist dagegen in jedem Kontext sicher.
  {
    // Sektoren löschen
    uint32_t sectorsNeeded = ((dataSize + 16 + FP_FLASH_SECTOR_SIZE - 1) / FP_FLASH_SECTOR_SIZE);
    for (uint32_t i = 0; i < sectorsNeeded; i++) {
      uint32_t sectorAddr = (offset + i * FP_FLASH_SECTOR_SIZE) / FP_FLASH_SECTOR_SIZE;

      bool ok;
      {
        CriticalSection cs;
        ok = ESP.flashEraseSector(sectorAddr);
      }
      if (!ok) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Erase failed"));
      }
      ESP.wdtFeed();
    }

    // Header schreiben
    {
      bool ok;
      {
        CriticalSection cs;
        ok = ESP.flashWrite(offset, (uint32_t*)header, 16);
      }
      if (!ok) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Write header failed"));
      }
    }

    // Daten in Blöcken schreiben (textData liegt im RAM)
    const char* dataPtr = textData.c_str();
    uint32_t written = 0;
    uint32_t writeOffset = offset + 16;

    while (written < dataSize) {
      uint32_t chunk[256];
      uint32_t chunkSize = min((uint32_t)1024, dataSize - written);
      memcpy(chunk, dataPtr + written, chunkSize);
      uint32_t alignedSize = (chunkSize + 3) & ~3;

      bool ok;
      {
        CriticalSection cs;
        ok = ESP.flashWrite(writeOffset, chunk, alignedSize);
      }
      if (!ok) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Write failed"));
      }

      written += chunkSize;
      writeOffset += alignedSize;
      ESP.wdtFeed();
    }
  }

  LOG_INFO(F("FlashPers"), F("Erfolgreich gespeichert"));
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

  // Prüfsumme verifizieren, BEVOR irgendetwas in die Preferences geschrieben
  // wird. Der Datenblock wird dafür in einem eigenen Durchgang gelesen und die
  // CRC32 fortlaufend mitgeführt - ohne den kompletten Puffer zu allokieren.
  //
  // Vorher wurde die CRC beim Speichern berechnet und abgelegt, beim
  // Wiederherstellen aber ausdrücklich übersprungen ("CRC check skipped").
  // Die 1-KB-Tabelle im Flash diente damit einem Wert, den nie jemand geprüft
  // hat, und beschädigte Konfigurationsdaten wären ungefiltert in die
  // Preferences gewandert.
  if (isChecksumVerificationEnabled()) {
    uint32_t running = 0xFFFFFFFF;
    uint32_t verifyOffset = offset + 16;
    uint32_t verified = 0;

    while (verified < dataSize) {
      char chunk[256];
      uint32_t chunkSize = min((uint32_t)256, dataSize - verified);
      uint32_t alignedSize = (chunkSize + 3) & ~3;

      if (!ESP.flashRead(verifyOffset, (uint32_t*)chunk, alignedSize)) {
        Serial.println(F("[FlashPers] FEHLER: Lesen für Prüfsumme fehlgeschlagen"));
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("CRC read failed"));
      }

      running = Crc32::update(running, (const uint8_t*)chunk, chunkSize);
      verified += chunkSize;
      verifyOffset += alignedSize;
      ESP.wdtFeed();
    }

    uint32_t computed = ~running;
    if (computed != storedCRC) {
      Serial.print(F("[FlashPers] FEHLER: Pruefsumme falsch - erwartet 0x"));
      Serial.print(storedCRC, HEX);
      Serial.print(F(", berechnet 0x"));
      Serial.println(computed, HEX);
      return ResourceResult::fail(ResourceError::DATA_CORRUPTION, F("CRC mismatch"));
    }
    Serial.println(F("[FlashPers] Pruefsumme OK"));
  } else {
    Serial.println(F("[FlashPers] Pruefsummen-Verifikation deaktiviert"));
  }

  // Daten zeilenweise in kleinen Blöcken lesen und parsen (kein großer Puffer)
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

                    // long ist auf dieser Plattform 32 Bit, die frühere obere
                    // Schranke (val <= 4294967295L) war deshalb immer wahr und
                    // der else-Zweig unerreichbar - negative Werte landeten
                    // trotzdem korrekt dort, weil val >= 0 zuerst geprüft wird.
                    // Jetzt ohne die tote Vergleichshälfte.
                    if (val >= 0 && val <= 255) {
                      prefs.putUChar(key, (uint8_t)val);
                    } else if (val >= 0) {
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
  LOG_INFO(F("FlashPers"), F("Lösche Flash..."));

  uint32_t offset = getSafeOffset();
  if (offset == 0) {
    return ResourceResult::success();
  }

  uint32_t sectorAddr = offset / FP_FLASH_SECTOR_SIZE;
  if (!ESP.flashEraseSector(sectorAddr)) {
    return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Erase failed"));
  }

  LOG_INFO(F("FlashPers"), F("Gelöscht"));
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
  LOG_INFO(F("FlashPers"), F("Sichere Preferences + Config-Dateien..."));

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
    LOG_WARN(F("FlashPers"), F("JSON-Sicherung fehlgeschlagen"));
    return jsonResult;
  }

  LOG_INFO(F("FlashPers"), F("Erfolgreich gespeichert (Preferences + JSON-Configs)"));
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
  LOG_INFO(F("FlashPers"), F("Sichere JSON-Configs in Flash..."));

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

#ifdef ESP32
  File root = LittleFS.open("/config");
  if (root && root.isDirectory()) {
    File entry = root.openNextFile();
    while (entry && fileCount < 16) {
      String filename = String(entry.name());
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash >= 0) {
        filename = filename.substring(lastSlash + 1);
      }
      // .txt gehört dazu: /config/mailvorlagen.txt trägt die bearbeitbaren
      // Mailvorlagen. Ohne diese Erweiterung wären sie nach jedem
      // Dateisystem-Update wieder auf Werkseinstellung, ohne dass es auffällt.
      if ((filename.endsWith(".json") || filename.endsWith(".txt")) &&
          !filename.endsWith(".example")) {
        files[fileCount].filename = filename;
        files[fileCount].size = entry.size();
        fileCount++;
      }
      entry.close();
      entry = root.openNextFile();
    }
    root.close();
  }
#else
  Dir dir = LittleFS.openDir("/config");
  while (dir.next() && fileCount < 16) {
    String filename = dir.fileName();
    // Siehe oben: .txt trägt die Mailvorlagen.
    if ((filename.endsWith(".json") || filename.endsWith(".txt")) &&
        !filename.endsWith(".example")) {
      File f = LittleFS.open("/config/" + filename, "r");
      if (f) {
        files[fileCount].filename = filename;
        files[fileCount].size = f.size();
        fileCount++;
        f.close();
      }
    }
  }
#endif

  if (fileCount == 0) {
    LOG_INFO(F("FlashPers"), F("Keine JSON-Dateien zum Sichern"));
    return ResourceResult::success();
  }

  LOG_INFO(F("FlashPers"), String(fileCount) + F(" JSON-Dateien gefunden"));

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

  LOG_INFO(F("FlashPers"), String(F("JSON Gesamt: ")) + String(totalSize) + F(" Bytes"));

  if (totalSize > FP_JSON_MAX_SIZE) {
    return ResourceResult::fail(ResourceError::INSUFFICIENT_SPACE, F("JSON too large"));
  }

  uint32_t manifestSize = manifest.length();

  // STEP 3: Prepare header (WiFi ON, safe)
  // Prüfsumme über Manifest + alle Dateiinhalte in einem Vorlauf berechnen.
  // Vorher stand hier eine feste 0 als Platzhalter - die Prüfsumme war damit
  // wertlos. Der Vorlauf liest die Dateien einmal zusätzlich aus LittleFS;
  // bei gut einem Kilobyte Gesamtdaten fällt das nicht ins Gewicht und
  // erspart es, den Header nachträglich an einer nicht ausgerichteten
  // Adresse überschreiben zu müssen.
  uint32_t running = Crc32::update(0xFFFFFFFF, (const uint8_t*)manifest.c_str(), manifestSize);
  for (uint8_t i = 0; i < fileCount; i++) {
    File cf = LittleFS.open("/config/" + files[i].filename, "r");
    if (!cf) {
      continue;
    }
    uint8_t buf[128];
    while (cf.available()) {
      size_t got = cf.read(buf, sizeof(buf));
      if (got == 0) {
        break;
      }
      running = Crc32::update(running, buf, got);
    }
    cf.close();
    ESP.wdtFeed();
  }
  const uint32_t jsonCrc = ~running;

  uint8_t header[16];
  memcpy(header, &FP_MAGIC_NUMBER, 4);
  header[4] = FP_VERSION;
  memcpy(header + 5, &manifestSize, 4);
  memcpy(header + 9, &jsonCrc, 4);
  memset(header + 13, 0, 3);

  uint32_t sectorsNeeded = ((totalSize + FP_FLASH_SECTOR_SIZE - 1) / FP_FLASH_SECTOR_SIZE);
  LOG_DEBUG(F("FlashPers"), String(F("Lösche ")) + String(sectorsNeeded) + F(" Sektoren..."));

  // Header und Manifest schreiben. Wie oben gilt: Interrupts nur um den
  // einzelnen Flash-Aufruf sperren, dazwischen Watchdog füttern.
  {
    // Alle benötigten Sektoren löschen
    for (uint32_t i = 0; i < sectorsNeeded; i++) {
      uint32_t sectorAddr = (offset + i * FP_FLASH_SECTOR_SIZE) / FP_FLASH_SECTOR_SIZE;
      bool ok;
      {
        CriticalSection cs;
        ok = ESP.flashEraseSector(sectorAddr);
      }
      if (!ok) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("JSON erase failed"));
      }
      ESP.wdtFeed();
    }

    // Header schreiben
    uint32_t writeOffset = offset;
    {
      bool ok;
      {
        CriticalSection cs;
        ok = ESP.flashWrite(writeOffset, (uint32_t*)header, 16);
      }
      if (!ok) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Header write failed"));
      }
    }
    writeOffset += 16;

    // Manifest in Blöcken schreiben
    const char* manifestPtr = manifest.c_str();
    uint32_t manifestWritten = 0;

    while (manifestWritten < manifestSize) {
      uint32_t chunk[256];
      uint32_t chunkSize = min((uint32_t)1024, manifestSize - manifestWritten);
      memcpy(chunk, manifestPtr + manifestWritten, chunkSize);
      uint32_t alignedSize = (chunkSize + 3) & ~3;

      bool ok;
      {
        CriticalSection cs;
        ok = ESP.flashWrite(writeOffset, chunk, alignedSize);
      }
      if (!ok) {
        return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("Manifest write failed"));
      }

      manifestWritten += chunkSize;
      writeOffset += alignedSize;
      ESP.wdtFeed();
    }
  }

  // STEP 6: Write file contents one by one (each in its own critical section)
  uint32_t writeOffset = offset + 16 + ((manifestSize + 3) & ~3);

  for (uint8_t fileIdx = 0; fileIdx < fileCount; fileIdx++) {
    String filepath = "/config/" + files[fileIdx].filename;

    // Open and read file (WiFi ON, interrupts enabled, safe for LittleFS)
    File f = LittleFS.open(filepath, "r");
    if (!f) {
      LOG_WARN(F("FlashPers"), String(F("Konnte nicht öffnen: ")) + files[fileIdx].filename);
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
                                      String(F("File write failed: ")) + files[fileIdx].filename);
        }
      }

      bytesRead += actualRead;
      writeOffset += alignedSize;
    }

    f.close();
    LOG_DEBUG(F("FlashPers"), String(F("Gesichert: ")) + files[fileIdx].filename);
  }

  LOG_INFO(F("FlashPers"), F("JSON-Configs erfolgreich in Flash gesichert"));
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

  uint32_t manifestSize, storedJsonCRC;
  memcpy(&manifestSize, header + 5, 4);
  memcpy(&storedJsonCRC, header + 9, 4);

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

  // Gespeicherte Prüfsumme über Manifest + Dateiinhalte verifizieren, bevor
  // irgendetwas nach LittleFS geschrieben wird.
  //
  // WICHTIG: Die Dateien liegen im Flash NICHT lückenlos hintereinander. Beim
  // Schreiben wird jeder Block auf 4 Byte aufgerundet (ESP.flashWrite verlangt
  // ausgerichtete Längen), die letzte Portion jeder Datei also mit 0xFF
  // aufgefüllt. Die Prüfsumme läuft nur über die echten Dateibytes, deshalb
  // muss hier dieselbe blockweise Schrittfolge nachvollzogen werden wie beim
  // Schreiben und beim eigentlichen Wiederherstellen.
  if (isChecksumVerificationEnabled()) {
    uint32_t running = Crc32::update(0xFFFFFFFF, (const uint8_t*)manifest.c_str(), manifestSize);
    uint32_t scanOffset = readOffset;

    int ls = manifest.indexOf('\n') + 1;
    while (ls > 0 && ls < (int)manifest.length()) {
      int le = manifest.indexOf('\n', ls);
      if (le == -1) {
        break;
      }
      int pipe = manifest.indexOf('|', ls);
      if (pipe > 0 && pipe < le) {
        size_t fileSize = manifest.substring(pipe + 1, le).toInt();
        size_t fileRead = 0;
        while (fileRead < fileSize) {
          uint32_t alignedBuffer[32];
          size_t chunkSize = (fileSize - fileRead > 128) ? 128 : (fileSize - fileRead);
          uint32_t alignedSize = (chunkSize + 3) & ~3;
          if (!ESP.flashRead(scanOffset, alignedBuffer, alignedSize)) {
            Serial.println(F("[FlashPers] FEHLER: Lesen für JSON-Pruefsumme fehlgeschlagen"));
            return ResourceResult::fail(ResourceError::OPERATION_FAILED, F("JSON CRC read failed"));
          }
          running = Crc32::update(running, (const uint8_t*)alignedBuffer, chunkSize);
          fileRead += chunkSize;
          scanOffset += alignedSize;
          ESP.wdtFeed();
        }
      }
      ls = le + 1;
    }

    uint32_t computed = ~running;
    if (computed != storedJsonCRC) {
      Serial.print(F("[FlashPers] FEHLER: JSON-Pruefsumme falsch - erwartet 0x"));
      Serial.print(storedJsonCRC, HEX);
      Serial.print(F(", berechnet 0x"));
      Serial.println(computed, HEX);
      return ResourceResult::fail(ResourceError::DATA_CORRUPTION, F("JSON CRC mismatch"));
    }
    Serial.println(F("[FlashPers] JSON-Pruefsumme OK"));
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
