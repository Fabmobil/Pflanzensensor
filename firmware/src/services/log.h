/**
 * @file log.h
 * @brief Logging-API (refactor.md 5.11)
 * @details LOGI(Mod::Wifi, "Verbunden mit %s (%d dBm)", ssid, rssi);
 *          Level und Debug-Bereich werden vor dem Formatieren geprüft, das
 *          Format liegt im Flash. Ausgabe an Serial, Datei-Puffer und den
 *          Live-Ring (siehe LogService).
 *
 *          Bewusste Ausnahme von "keine Globals": der Logger hängt über
 *          log::attach() am Composition Root, damit jedes Modul ihn ohne
 *          Konstruktorparameter benutzen kann - wie Serial.
 */
#pragma once

#include <stdarg.h>

#include "core/config/device_config.h"
#include "platform/progmem.h"

namespace ps {

enum class Mod : uint8_t {
  Main = 0,
  Boot,
  Config,
  Sensor,
  Cycle,
  Display,
  Led,
  Wifi,
  Time,
  Web,
  Auth,
  Mail,
  Chronik,
  Ota,
  Mem,
  Fs,
  Count
};

/// Debug-Bereiche (Schalter in der Konfiguration)
enum class DebugArea : uint8_t { None, Ram, Cycle, Sensor, Display, Web, Mail };

namespace log {

class Backend {
public:
  virtual bool enabled(LogLevel level, Mod mod) const = 0;
  virtual void vwrite(LogLevel level, Mod mod, const char* fmtP, va_list ap) = 0;

protected:
  ~Backend() = default;
};

void attach(Backend* b);
bool enabled(LogLevel level, Mod mod);
void write(LogLevel level, Mod mod, const char* fmtP, ...) __attribute__((format(printf, 3, 4)));

/// Kurzname eines Moduls (im Flash)
FStr modName(Mod m);
DebugArea areaOf(Mod m);

} // namespace log
} // namespace ps

#define PS_LOG(level, mod, fmt, ...)                                                               \
  do {                                                                                             \
    if (::ps::log::enabled(level, mod))                                                            \
      ::ps::log::write(level, mod, PS_PSTR(fmt), ##__VA_ARGS__);                                   \
  } while (0)

#define LOGD(mod, fmt, ...) PS_LOG(::ps::LogLevel::Debug, mod, fmt, ##__VA_ARGS__)
#define LOGI(mod, fmt, ...) PS_LOG(::ps::LogLevel::Info, mod, fmt, ##__VA_ARGS__)
#define LOGW(mod, fmt, ...) PS_LOG(::ps::LogLevel::Warning, mod, fmt, ##__VA_ARGS__)
#define LOGE(mod, fmt, ...) PS_LOG(::ps::LogLevel::Error, mod, fmt, ##__VA_ARGS__)
