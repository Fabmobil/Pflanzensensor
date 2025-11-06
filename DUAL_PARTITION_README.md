# Dual LittleFS Partition Implementation

## Overview

This implementation creates a dual LittleFS partition system for the ESP8266-based Pflanzensensor project. The system ensures that configuration settings persist across OTA filesystem updates by storing them on a separate flash partition.

## Architecture

### Flash Memory Layout (4MB ESP8266)

```
Address Range          Size      Partition    Purpose
================================================================================
0x40200000-0x40389000  ~1575KB   Sketch       Firmware (current)
0x40389000-0x40512000  ~1575KB   OTA          Firmware (update)
0x40510000-0x40520000  64KB      CONFIG       Preferences storage (PROTECTED)
0x40520000-0x405F3000  ~844KB    MAIN_FS      Web assets, logs (OTA updatable)
0x405F3000-0x405F4000  4KB       EEPROM       Traditional EEPROM
0x405FB000-0x405FC000  4KB       RF Cal       RF calibration data
0x405FD000-0x40600000  12KB      WiFi         WiFi configuration
```

### Key Design Principles

1. **CONFIG Partition (64KB)**
   - Mounted as the global `LittleFS` object
   - Used by vshymanskyy/Preferences library automatically
   - Stores all user settings (WiFi, sensors, display, etc.)
   - **NEVER** touched by OTA updates
   - Survives filesystem OTA updates

2. **MAIN_FS Partition (~844KB)**
   - Mounted as separate `MainFS` object
   - Stores web assets (HTML, CSS, JS)
   - Stores log files and temporary data
   - **Updated** during filesystem OTA operations
   - Can be safely wiped without losing settings

## Implementation Details

### Phase 1: Flash Layout Modification

**File:** `eagle.flash.4mmore.ld`

- Created CONFIG partition: 64KB @ 0x40510000-0x40520000
- Created MAIN_FS partition: ~844KB @ 0x40520000-0x405F3000
- Reduced EEPROM from 16KB to 4KB to make room
- Updated memory layout symbols (`_CONFIG_start`, `_CONFIG_end`, `_FS_start`, `_FS_end`)

### Phase 2: Dual Filesystem Manager

**Files:** `src/filesystem/config_fs.h`, `src/filesystem/config_fs.cpp`

Created `DualFS` class that:
- Mounts CONFIG partition FIRST as global `LittleFS` (for Preferences)
- Mounts MAIN_FS partition as separate `MainFS` object (for web assets)
- Provides separate info/format methods for each partition
- Handles initialization errors gracefully

**Integration:** `src/main.cpp`

- Dual filesystem initialized BEFORE any Preferences usage
- Proper error handling and logging
- Both partitions mounted at startup

### Phase 3: OTA Protection

**File:** `src/web/handler/web_ota_handler.cpp`

- Filesystem OTA updates ONLY target MAIN_FS partition
- CONFIG partition never touched by `Update.begin(size, U_FS)`
- Removed all backup/restore logic (no longer needed!)
- Added logging to confirm partition protection

**File:** `src/managers/manager_config.cpp`

- Removed `backupPreferencesToFile()` calls
- Added comments explaining dual partition protection
- Update flags stored on MAIN_FS (temporary data)

**File:** `src/managers/manager_config_persistence.cpp`

- Removed ~300 lines of backup/restore code
- Update flags use MainFS instead of LittleFS

### Phase 4: File Operation Migration

All file operations updated to use the correct partition:

**Web Assets (use MainFS):**
- `src/web/services/css_service.cpp` - CSS files
- `src/web/core/web_manager_static.cpp` - Static file serving

**Logs and Temporary Files (use MainFS):**
- `src/logger/logger.cpp` - Log file operations
- `src/web/handler/admin_handler_*.cpp` - Log viewing
- `src/utils/helper.cpp` - Reboot count
- `src/utils/persistence_utils.cpp` - Generic file utilities

**Images (use MainFS):**
- `src/display/display.cpp` - Display images

**Configuration (automatic via Preferences):**
- All Preferences data automatically stored on CONFIG partition
- No code changes needed - just works!

## Benefits

### ✅ Settings Survive OTA Updates

- WiFi credentials persist across filesystem updates
- Sensor calibrations preserved
- Display preferences maintained
- All user settings protected

### ✅ Simplified Code

- Removed ~300 lines of backup/restore code
- No complex state management
- Cleaner OTA handler
- Fewer failure modes

### ✅ Better Reliability

- No backup file corruption risk
- No restore failures
- Atomic operations (either works or doesn't)
- Clear separation of concerns

### ✅ Transparent Integration

- Preferences library works unchanged
- No API modifications needed
- Existing code compatible
- Minimal changes required

## Usage

### Normal Operation

The dual partition system is completely transparent to normal operation:

```cpp
// Preferences automatically use CONFIG partition
Preferences prefs;
prefs.begin("wifi1", false);
prefs.putString("ssid", "MyNetwork");  // Stored on CONFIG
prefs.end();

// Web assets use MAIN_FS
File cssFile = MainFS.open("/css/style.css", "r");  // From MAIN_FS
String css = cssFile.readString();
cssFile.close();
```

### OTA Updates

**Firmware OTA:**
- Updates sketch partition
- CONFIG and MAIN_FS untouched
- Settings automatically preserved

**Filesystem OTA:**
- Updates ONLY MAIN_FS partition
- CONFIG partition protected
- Settings automatically preserved
- Web assets updated

**Settings Reset:**
- Admin page "Einstellungen zurücksetzen" button
- Calls `ConfigManager::resetToDefaults()`
- Clears CONFIG partition Preferences
- Preserves MAIN_FS data

## Testing

### Test Scenarios

1. **Fresh Flash**
   ```
   - Flash new firmware
   - Configure WiFi and sensors
   - Reboot
   → Settings persist ✓
   ```

2. **Filesystem OTA**
   ```
   - Configure custom settings
   - Upload littlefs.bin via OTA
   - Reboot after update
   → Settings persist ✓
   ```

3. **Firmware OTA**
   ```
   - Configure custom settings  
   - Upload firmware.bin via OTA
   - Reboot after update
   → Settings persist ✓
   ```

4. **Settings Reset**
   ```
   - Navigate to Admin page
   - Click "Einstellungen zurücksetzen"
   - Confirm reset
   → Settings cleared, defaults loaded ✓
   ```

### Verification Logs

Look for these log messages to verify correct operation:

```
[DualFS] CONFIG Partition gemountet (für Preferences)
[DualFS] MAIN_FS Partition gemountet (für Web-Assets)
[WebOTAHandler] Filesystem-Update: Nur MAIN_FS wird aktualisiert
[WebOTAHandler] CONFIG Partition (Preferences) bleibt geschützt
```

## Migration Notes

### From Previous Implementation

If upgrading from a previous version with backup/restore:

1. **First boot after upgrade:**
   - CONFIG partition will be empty (formatted)
   - Preferences will initialize with defaults
   - User will need to re-enter settings
   - This is a one-time migration

2. **Subsequent boots:**
   - Settings persist normally
   - OTA updates preserve settings
   - No manual intervention needed

### Compatibility

- **Forward compatible:** New system works with existing data
- **Backward compatible:** Old firmware cannot read new layout
- **Recommended:** Flash new firmware + filesystem together first time

## Troubleshooting

### CONFIG Partition Not Mounting

**Symptoms:** Settings don't persist after reboot

**Solution:**
```cpp
// Check logs for:
[DualFS] CONFIG mount fehlgeschlagen, formatiere...
[DualFS] CONFIG Partition erfolgreich formatiert
```

If formatting fails, re-flash firmware with erase_flash option.

### MAIN_FS Partition Issues

**Symptoms:** Web interface not loading, missing CSS/images

**Solution:**
```cpp
// Check logs for:
[DualFS] MAIN_FS mount fehlgeschlagen, formatiere...
```

Upload littlefs.bin via OTA to restore web assets.

### Settings Lost After OTA

**This should NOT happen.** If it does:

1. Check that firmware uses this dual partition implementation
2. Verify linker script has correct addresses
3. Check logs for CONFIG partition errors
4. File an issue with logs

## Future Enhancements

Potential improvements:

1. **Automatic migration:** Detect old Preferences and migrate to CONFIG
2. **Config export/import:** Download/upload settings as JSON  
3. **Partition stats:** Show CONFIG/MAIN_FS usage on admin page
4. **Integrity checks:** Verify CONFIG partition health on boot

## Credits

- Original concept: Dual partition for OTA protection
- Implementation: GitHub Copilot + tommyschoen
- Library: vshymanskyy/Preferences @ ^2.1.0
- Platform: ESP8266 Arduino Core

## References

- ESP8266 Flash Memory Layout: https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html
- LittleFS Documentation: https://github.com/littlefs-project/littlefs
- Preferences Library: https://github.com/vshymanskyy/Preferences
