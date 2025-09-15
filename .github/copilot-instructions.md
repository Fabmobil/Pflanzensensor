# AI coding guide for this repo (ESP8266/PlatformIO)

Goal: help AI agents make correct, memory-safe edits fast. Keep changes small, respect feature flags, and reuse existing result/logging patterns.

## Project shape and build
- Platform: PlatformIO Arduino for ESP8266 (NodeMCU v2). Sources: `Pflanzensensor/src`, web assets: `Pflanzensensor/data` (LittleFS).
- Build config: `platformio.ini` — default env `env:default`, `board=nodemcuv2`, `platform=espressif8266@^3.2.0`, LittleFS, ldscript `eagle.flash.4mmore.ld`.
- Compile-time config injected via `-D CONFIG_FILE="configs/config_pflanzensensor.h"`.
- Extra scripts: `generate_md5.py` (post-action MD5 for firmware/fs), `error_parser.py` (pretty error summary). Serial: 115200, upload: 460800.

## Runtime architecture
- Entry: `src/main.cpp` orchestrates init via `Helper::initializeComponent(...)` returning `ResourceResult`.
- Config: `managers/manager_config.*` (singleton `ConfigMgr`) handles persistence, validation, debug flags, and update flags.
- Sensors: `managers/manager_sensor.*` + `sensors/sensor_factory.*` create sensors behind feature flags (`USE_DHT`, `USE_ANALOG`, ...). Per-sensor cycles via `SensorMeasurementCycleManager`.
- Web: `web/core/web_manager.*` (singleton) + `web/handler/*` routes. Minimal “update mode” for OTA: only essential routes.
- WiFi: `utils/wifi.*` multi-SSID connect, AP/captive portal detection, status strings for display.
- Display: `display/*` (SSD1306 via Adafruit) shown via `manager_display.*` with boot/update/status screens.
- Logging: `logger/logger.*` leveled logs, NTP time, memory stats, optional file logging, WebSocket streaming.
- InfluxDB (optional): `influxdb/*` gated by `USE_INFLUXDB`.

## Flow of setup/loop (big picture)
- Boot: mount LittleFS → ensure default JSONs → load config → if `ConfigMgr.getDoFirmwareUpgrade()` then start minimal web/OTA and return early.
- Normal mode: WiFi → NTP → SensorManager → WebManager routes → optional InfluxDB. Loop: WebSocket, sensor updates (~1s min), display update, WiFi health (30s), maintenance (10 min), yield/delay(1).

## Conventions and patterns
- Feature flags from `configs/config_pflanzensensor.h` gate compilation and codepaths (`USE_WIFI`, `USE_WEBSERVER`, sensors...). Guard includes with `#if` consistently.
- Results: use `utils/result_types.h` (`ResourceResult`, `SensorResult`, `RouterResult`, `DisplayResult`). Provide messages via `.fail(..., message)`.
- Logging: global `logger` with `F()` PROGMEM strings. Keep messages short; avoid heap churn.
- Timing: call `yield()`/`delay(1)` in long paths; respect `MEASUREMENT_*` macros.
- Static files: serve via `WebManager::serveStaticFile` (avoid `serveStatic` MD5 overhead on ESP8266).
- Language: all output shall be in german (UI, logs, errors). Update messages which are still in English once you find them.

## Config and data
- Compile-time: device name, pins, thresholds, intervals, SSIDs in `configs/config_pflanzensensor.h`. Version in `configs/config.h`.
- Runtime: defaults ensured by `configs/default_json_generator.*`; sensors load settings via `SensorPersistence::loadFromFile()` from `SensorManager::applySensorSettingsFromConfig()`.
- AP/captive portal: if not connected, AP is started; WiFi setup integrated in start page via `web/handler/wifi_setup_handler.*`.

## Extending (examples)
- New sensor: add `sensors/sensor_<name>.{h,cpp}` (implement `Sensor`), guard with `USE_<NAME>`, integrate in `sensor_factory.*`, add macros to `config_pflanzensensor.h`, extend admin UI if needed (`admin_sensor_handler_*`).
- New web route: create handler in `web/handler/` (subclass `BaseHandler`), register in `web_manager_*`, consider auth (`WebAuth`) and handler cache limits.

## Dev workflows
- Build/Upload: PlatformIO default env. LittleFS data: use PlatformIO FS build/upload tasks.
- Lint/Check: `pio check` via `cppcheck` (`check_tool`, `check_flags` set in `platformio.ini`).
- Debug: monitor at 115200 with exception decoder. Use `logger.logMemoryStats()` around heavy ops.
- OTA/update: set via web (`/admin/config/update`) or `ConfigMgr.setUpdateFlags(...)`; boot enters minimal mode with timeout (see `WebManager` update-mode members).

## Watch-outs
- RAM is tight: prefer PROGMEM (`F()`), avoid large stack buffers; keep handler cache small (`WebManager::MAX_ACTIVE_HANDLERS`).
- Respect init order in `main.cpp`: FS → config → display → WiFi → NTP → sensors → web → influx.
- Don’t bypass singletons; use `ConfigMgr`, `WebManager::getInstance()`, `sensorManager`, `displayManager` APIs.
