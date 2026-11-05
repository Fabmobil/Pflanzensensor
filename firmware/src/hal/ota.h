/**
 * @file ota.h
 * @brief Firmware-Update schreiben (Update-Bibliothek des Cores)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {
namespace hal {

class Ota {
public:
  /// size 0 = unbekannt (dann freier Sketch-Bereich); md5 optional (32 hex)
  virtual bool begin(uint32_t size, const char* md5) = 0;
  virtual size_t write(const uint8_t* data, size_t len) = 0;
  virtual bool end() = 0;
  virtual void abort() = 0;
  /// Letzte Fehlermeldung (kurzer Text)
  virtual const char* error() = 0;

protected:
  ~Ota() = default;
};

} // namespace hal
} // namespace ps
