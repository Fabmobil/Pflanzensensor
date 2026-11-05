/**
 * @file asset_types.h
 * @brief Eingebettete, gzip-komprimierte Web-Assets (E6)
 */
#pragma once

#include <stdint.h>

#include "platform/progmem.h"

namespace ps {
namespace assets {

struct Asset {
  char path[40];
  char type[32];
  char etag[12];
  const uint8_t* data; ///< gzip, im Flash
  uint32_t len;
  bool html; ///< HTML: Cache-Control no-cache
};

} // namespace assets
} // namespace ps
