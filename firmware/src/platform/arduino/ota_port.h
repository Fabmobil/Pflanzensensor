/**
 * @file ota_port.h
 * @brief hal::Ota auf der Update-Bibliothek des Cores
 */
#pragma once

#if defined(ARDUINO)

#include "hal/ota.h"

namespace ps {
namespace platform {

class OtaPort : public hal::Ota {
public:
  bool begin(uint32_t size, const char* md5) override;
  size_t write(const uint8_t* data, size_t len) override;
  bool end() override;
  void abort() override;
  const char* error() override { return err_; }

private:
  void capture();
  char err_[64] = "";
};

} // namespace platform
} // namespace ps

#endif
