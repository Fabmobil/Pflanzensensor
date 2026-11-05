/**
 * @file sha256.h
 * @brief SHA-256 (FIPS 180-4), portabel und ohne Heap
 * @details Für das Admin-Passwort (gesalzen) und den Vergleich mit dem
 *          Notfallpasswort, das nur als Hash im Image steht.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ps {

class Sha256 {
public:
  static constexpr size_t kDigest = 32;
  Sha256() { reset(); }
  void reset();
  void update(const uint8_t* data, size_t len);
  void update(const char* s) { update(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
  void finish(uint8_t out[kDigest]);

private:
  void block(const uint8_t* p);
  uint32_t h_[8];
  uint8_t buf_[64];
  uint64_t bits_;
  size_t used_;
};

/// Vergleich in konstanter Zeit
inline bool equalConstTime(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++)
    d |= static_cast<uint8_t>(a[i] ^ b[i]);
  return d == 0;
}

} // namespace ps
