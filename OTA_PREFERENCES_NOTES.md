# OTA Preferences Backup/Restore Notes

## Current Implementation

The OTA handler now includes complete backup/restore functionality for settings during filesystem updates:

### What Was Implemented

1. **Sensor Settings Backup** (`backupSensorSettings()` and `backupOneSensor()`)
   - Backs up all sensor configurations to backup namespaces (e.g., `s_ANALOG` → `s_bak_ANALOG`)
   - Supports up to 8 measurements for ANALOG sensors, 2 for DHT sensors
   - Copies all measurement settings including thresholds, calibration data, etc.

2. **Sensor Settings Restore** (`restoreSensorSettings()` and `restoreOneSensor()`)
   - Restores sensor configurations from backup namespaces after filesystem update
   - Cleans up backup namespaces after successful restore
   - Handles missing backups gracefully

3. **General Settings Backup/Restore**
   - Already implemented for WiFi, display, debug, LED, log settings
   - Uses in-memory struct `_prefsBackup` to store settings

### Critical Issue: Preferences Storage Location

**IMPORTANT**: The vshymanskyy/Preferences library v2.1.0 on ESP8266 stores data in LittleFS by default (in `/littlefs/preferences/` directory), NOT in EEPROM!

This means:
- ❌ Backup namespaces (s_bak_*) are ALSO stored in LittleFS
- ❌ They WILL be wiped out during filesystem updates
- ❌ The backup/restore won't work as currently implemented

### Solutions

#### Option 1: Configure Preferences to Use EEPROM (RECOMMENDED)

The flash layout already reserves 16KB for EEPROM (0x405F7000-0x405FB000). We need to configure the Preferences library to use this instead of LittleFS.

**Steps**:
1. Check if vshymanskyy/Preferences has a compile flag to use EEPROM
2. Add the flag to `platformio.ini` build_flags
3. Verify settings survive filesystem updates

#### Option 2: Use ESP8266 EEPROM Library Directly

Bypass the Preferences library for backup and use ESP8266's native EEPROM library:

```cpp
#include <EEPROM.h>

void backupToEEPROM() {
  EEPROM.begin(4096);  // Use EEPROM partition
  // Serialize settings to EEPROM
  // ...
  EEPROM.commit();
  EEPROM.end();
}

void restoreFromEEPROM() {
  EEPROM.begin(4096);
  // Deserialize settings from EEPROM
  // ...
  EEPROM.end();
}
```

#### Option 3: Document Limitation

If filesystem updates are rare and settings can be re-entered manually, document that users should:
1. Export/backup settings manually before filesystem updates
2. Re-import/restore settings after filesystem updates
3. OR: Only do firmware updates via OTA, use serial/USB for filesystem updates

### Testing Requirements

To verify the solution works:
1. Configure sensor with custom settings
2. Trigger filesystem OTA update
3. Verify settings survive the update
4. Check that all sensor measurements, thresholds, and calibration data are preserved

### Current Status

- ✅ Backup/restore code is complete and handles all sensor types
- ❌ Storage mechanism needs to be changed from LittleFS-based Preferences to EEPROM
- ⏳ Hardware testing needed to verify the solution

## Recommendation

Investigate adding a build flag to force vshymanskyy/Preferences to use EEPROM. If that's not possible, implement Option 2 (use EEPROM library directly for backups).
