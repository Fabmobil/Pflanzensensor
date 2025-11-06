# Pflanzensensor - Copilot Coding Agent Instructions

## Project Overview

**Pflanzensensor** is an ESP8266-based plant monitoring sensor system developed by Fabmobil. It monitors environmental conditions (soil moisture, light, temperature, humidity) and provides a web interface for configuration and data visualization. The project is written in **German** - all comments, logs, documentation, and UI text are in German.

**Repository Statistics:**
- ~126 source files (C++ and headers)
- Source size: ~1.3MB
- Languages: C++ (Arduino framework), Python (build scripts), HTML/CSS/JavaScript (web interface)
- Target Platform: ESP8266 (NodeMCU v2)
- Framework: Arduino/PlatformIO

## Critical Build Information

### Build System: PlatformIO

**IMPORTANT:** This project uses PlatformIO, NOT Arduino IDE. Always use `pio` commands.

#### Install PlatformIO (if not available):
```bash
pip3 install platformio
```

#### Build Commands:

**Compile firmware:**
```bash
pio run --environment default
```
Time: ~2-5 minutes on first build (downloads platforms & libraries), ~30-60 seconds for subsequent builds.

**Compile debug build:**
```bash
pio run --environment debug
```

**Build filesystem image:**
```bash
pio run --target buildfs
```

**Check code with cppcheck:**
```bash
pio check --environment default
```

**Clean build:**
```bash
pio run --target clean
```

**CRITICAL:** Network access is required for the first build to download:
- espressif8266 platform (v3.2.0)
- Arduino framework
- All library dependencies (see platformio.ini)

If build fails with "HTTPClientError", it indicates network connectivity issues. The build system cannot proceed without downloading dependencies.

### Build Configuration

**Main config file:** `platformio.ini`
- Source directory: `Pflanzensensor/src`
- Data directory: `Pflanzensensor/data` (web assets)
- Build directory: `.pio/build`
- Default environment: `default`
- Debug environment: `debug`

**Linker script:** `eagle.flash.4mmore.ld` (custom memory layout)
- Sketch: ~1575KB
- OTA: ~1575KB  
- LittleFS: 916KB
- EEPROM: 16KB (critical for config persistence!)
- RF Cal: 4KB
- WiFi: 12KB

### Build Scripts

**Pre-build:** `generate_md5.py` - Generates MD5 hashes for firmware and filesystem images
**Post-build:** `error_parser.py` - Parses and formats build errors/warnings

### Code Formatting

**ALWAYS run clang-format before committing!**

```bash
clang-format -i Pflanzensensor/src/**/*.cpp Pflanzensensor/src/**/*.h
```

Configuration: `.clang-format` (LLVM-based, 100 char line limit, 2-space indent)

**Pre-commit hooks:** Configured in `.pre-commit-config.yaml`
- Automatically runs clang-format
- Requires clean working tree before commit

To install hooks:
```bash
pip install pre-commit
pre-commit install
```

## Project Architecture

### Directory Structure

```
Pflanzensensor/
├── src/                          # Main source code
│   ├── main.cpp                  # Entry point, setup() and loop()
│   ├── configs/                  # Configuration headers
│   │   ├── config.h              # Main config includes
│   │   ├── config_example.h      # Default configuration values
│   │   └── config_validation_rules.h
│   ├── managers/                 # Manager classes (resource, sensor, config, display, LED)
│   │   ├── manager_config*.cpp/h # Configuration management (26 files)
│   │   ├── manager_sensor*.cpp/h # Sensor management  
│   │   ├── manager_display*.cpp/h
│   │   ├── manager_led_traffic_light*.cpp/h
│   │   └── manager_resource*.cpp/h
│   ├── sensors/                  # Sensor implementations
│   │   ├── sensor_analog*.cpp/h  # Analog sensors (soil moisture, light)
│   │   ├── sensor_dht*.cpp/h     # DHT temperature/humidity sensors
│   │   ├── sensor_factory*.cpp/h # Sensor factory pattern
│   │   └── sensor_measurement*.cpp/h # Measurement cycle logic
│   ├── web/                      # Web server components
│   │   ├── core/                 # Web manager, router, auth
│   │   ├── handler/              # Request handlers (admin, logs, etc.)
│   │   └── services/             # WebSocket service
│   ├── display/                  # OLED display (SSD1306)
│   ├── led-traffic-light/        # LED traffic light indicator
│   ├── logger/                   # Logging system
│   ├── filesystem/               # Dual filesystem (CONFIG + MAIN_FS)
│   └── utils/                    # Utilities, WiFi, persistence
├── data/                         # Web interface assets (uploaded to LittleFS)
│   ├── css/, js/, img/           # Web UI resources
│   └── favicon.ico
├── Dokumentation/                # Documentation, datasheets, 3D models
├── platformio.ini                # PlatformIO configuration
├── generate_md5.py               # Build script (MD5 generation)
├── error_parser.py               # Build script (error parsing)
├── eagle.flash.4mmore.ld         # Custom linker script (EEPROM layout)
├── .clang-format                 # Code formatting rules
├── .pre-commit-config.yaml       # Pre-commit hooks
└── .gitignore                    # Git ignore rules
```

### Key Architectural Components

**1. Dual Filesystem Architecture (`filesystem/config_fs.h`)**
- **CONFIG Partition:** For Preferences (EEPROM-backed, survives OTA updates)
- **MAIN_FS Partition:** For web assets (updated during OTA)
- CRITICAL: CONFIG partition MUST be mounted first as global LittleFS

**2. Configuration Management (EEPROM-based)**
- Uses custom `PreferencesEEPROM` implementation (stores in 16KB EEPROM)
- **NOT** using vshymanskyy/Preferences (which uses LittleFS)
- Configuration survives filesystem OTA updates
- See `PREFERENCES_DOCUMENTATION.md` and `EEPROM_SOLUTION.md` for details

**3. Manager Pattern:**
- `ResourceManager`: Memory and resource tracking
- `SensorManager`: Manages all sensors, measurement cycles
- `ConfigManager`: Handles configuration (load/save/validate)
- `DisplayManager`: Controls OLED display
- `LedTrafficLightManager`: Controls LED indicators
- `WebManager`: Web server and routes

**4. Sensor System:**
- Factory pattern for sensor creation
- Supports ANALOG (multiplexed) and DHT sensors
- Measurement cycle state machine
- Auto-calibration support
- Error handling and retry logic

## Configuration Files

### Critical Config: `config_pflanzensensor.h`

**NEVER commit this file!** It's in `.gitignore`.

Use `config_example.h` as reference for default values. The config includes:
- Device name, admin password
- WiFi credentials (supports 3 SSIDs)
- Feature flags (USE_WIFI, USE_WEBSERVER, USE_DISPLAY, etc.)
- Debug flags (DEBUG_RAM, DEBUG_SENSOR, etc.)
- Sensor configuration (pins, thresholds, intervals)

### Configuration Namespaces (Preferences/EEPROM)

**General:** `general` - device name, admin password, flags
**WiFi:** `wifi` - SSIDs and passwords
**Display:** `display` - screen preferences, duration
**Log:** `log` - log level, file logging
**LED:** `led_traf` - LED mode and selection
**Debug:** `debug` - debug flags
**Sensors:** `s_<sensorId>` - per-sensor configuration (e.g., `s_ANALOG_1`, `s_DHT_1`)

## Common Development Workflows

### Making Code Changes

1. **Locate the relevant file:**
   - Sensor logic: `Pflanzensensor/src/sensors/`
   - Web handlers: `Pflanzensensor/src/web/handler/`
   - Configuration: `Pflanzensensor/src/managers/manager_config*.cpp`
   - Main loop: `Pflanzensensor/src/main.cpp`

2. **Edit the code** following German naming conventions

3. **Format the code:**
   ```bash
   clang-format -i <file>.cpp <file>.h
   ```

4. **Build and verify:**
   ```bash
   pio run --environment default
   ```

5. **Check for issues:**
   ```bash
   pio check
   ```

### Adding a New Feature

1. Update relevant config in `config_example.h`
2. Implement in appropriate manager or component
3. Add German log messages via `logger` class
4. Update web interface if needed (HTML in `web/handler/*.cpp`, assets in `data/`)
5. Format, build, and test

### Modifying Web Interface

**Backend:** `Pflanzensensor/src/web/`
- Routes: `web/core/web_manager_routes.cpp`
- Handlers: `web/handler/*.cpp`
- Auth: `web/core/web_auth.cpp`

**Frontend:** `Pflanzensensor/data/`
- HTML served from handlers (embedded in C++ strings)
- CSS: `data/css/*.css`
- JavaScript: `data/js/*.js`
- Images: `data/img/*.{png,gif}`

**Upload web assets to device:**
```bash
pio run --target uploadfs
```

## Testing

**IMPORTANT:** This project has NO automated test suite. Manual testing required.

### Manual Testing Checklist:

1. **Build succeeds:** `pio run --environment default`
2. **Code formatting:** `clang-format` produces no changes
3. **Static analysis:** `pio check` reports no new errors
4. **Functionality testing:** Flash to device and verify behavior

## Important Implementation Notes

### Memory Management
- ESP8266 has LIMITED RAM (~80KB available)
- ALWAYS check heap usage when adding features
- Use `F()` macro for string constants in flash
- Prefer `std::unique_ptr` over raw pointers
- Monitor heap via debug flags (`DEBUG_RAM`)

### EEPROM Configuration Persistence
- Configuration stored in 16KB EEPROM partition
- Survives OTA firmware updates
- DOES NOT survive filesystem updates
- Maximum namespace name: 15 chars
- Maximum key name: 15 chars
- See `EEPROM_SOLUTION.md` for implementation details

### OTA Updates
- Firmware OTA: Preserves EEPROM config
- Filesystem OTA: Wipes web assets but preserves EEPROM
- MD5 verification for update safety
- See `OTA_PREFERENCES_NOTES.md`

### Logging
- All logs in **German**
- Logger class: `logger.info()`, `logger.error()`, etc.
- Log levels: DEBUG, INFO, WARNING, ERROR
- WebSocket streaming of logs to web interface
- File logging optional (saves to LittleFS)

### Measurement Cycles
- State machine implementation
- Configurable intervals (default 60s)
- Auto-calibration support for analog sensors
- Error retry logic (5 consecutive errors trigger reinit)
- See `sensors/sensor_measurement_cycle*.cpp`

## Common Pitfalls & Workarounds

### Build Issues

**Problem:** "HTTPClientError" during build
**Solution:** Network connectivity required for first build. Ensure internet access.

**Problem:** Linker errors about multiple definitions
**Solution:** Check `error_parser.py` output for specific symbols. Usually caused by missing `#pragma once` or extern declarations.

**Problem:** "undefined reference" errors
**Solution:** Missing library or implementation file. Check `lib_deps` in `platformio.ini`.

### Code Issues

**Problem:** Heap fragmentation / memory issues
**Solution:** 
- Use `logger.beginMemoryTracking()` and `logger.endMemoryTracking()`
- Minimize dynamic allocations in loop()
- Use static allocation where possible

**Problem:** WebSocket disconnects
**Solution:** Reduce debug output volume, increase WiFi stability checks

**Problem:** OLED display not updating
**Solution:** Check `USE_DISPLAY` flag, verify I2C pins (SDA/SCL), check display initialization logs

### Configuration Issues

**Problem:** Settings lost after OTA update
**Solution:** 
- Firmware OTA: Settings preserved in EEPROM ✓
- Filesystem OTA: Settings preserved in EEPROM ✓
- Full flash erase: Settings lost (expected)

**Problem:** Cannot access web interface
**Solution:** Check WiFi connection, verify SSID/password in config, check serial logs for IP address

## Documentation References

- `Readme.md` - Project overview and credits
- `PREFERENCES_DOCUMENTATION.md` - Configuration storage details
- `EEPROM_SOLUTION.md` - EEPROM implementation and memory layout
- `OTA_PREFERENCES_NOTES.md` - OTA update behavior
- `webinterface.md` - Web interface documentation (German)
- Wiki: https://github.com/Fabmobil/Pflanzensensor/wiki

## Development Environment

**Recommended tools:**
- PlatformIO Core or PlatformIO IDE (VSCode extension)
- clang-format (for code formatting)
- Git with pre-commit hooks
- Serial monitor for debugging (115200 baud)

**Optional (Nix users):**
```bash
nix-shell  # Loads dev environment from shell.nix
```

## Final Checklist Before Committing

- [ ] Code follows existing German naming conventions
- [ ] All strings use German language
- [ ] Ran `clang-format -i` on modified files
- [ ] Build succeeds: `pio run --environment default`
- [ ] No new warnings from `pio check`
- [ ] Tested functionality manually (if applicable)
- [ ] No sensitive data (passwords, SSIDs) in committed files
- [ ] Updated documentation if adding new features
- [ ] Commit message in English (convention)

## Trust These Instructions

These instructions are comprehensive and tested. Only perform additional searches if:
- Information here is incomplete for your specific task
- You encounter errors not covered here
- You need to understand implementation details beyond this overview

When in doubt, check the documentation files listed above or examine the existing code patterns in similar components.
