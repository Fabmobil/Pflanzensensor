/**
 * @file segments.h
 * @brief Namen und Reihenfolge der Segmentdateien (aus dem Altcode)
 * @details "/chr_0000002a.dat" im Wurzelverzeichnis; der Index steigt monoton.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ps {
namespace chronik {

constexpr size_t kNameLength = 17;
constexpr size_t kNameBuffer = kNameLength + 1;

inline void nameFromIndex(uint32_t index, char* out) {
  static const char kDigits[] = "0123456789abcdef";
  memcpy(out, "/chr_", 5);
  for (uint8_t i = 0; i < 8; i++)
    out[5 + i] = kDigits[(index >> ((7 - i) * 4)) & 0x0F];
  memcpy(out + 13, ".dat", 4);
  out[kNameLength] = '\0';
}

/// Index aus Dateinamen (mit oder ohne "/"), -1 wenn kein Segment
inline int64_t indexFromName(const char* name) {
  if (!name)
    return -1;
  const char* p = name[0] == '/' ? name + 1 : name;
  if (strncmp(p, "chr_", 4) != 0)
    return -1;
  p += 4;
  uint32_t idx = 0;
  for (uint8_t i = 0; i < 8; i++) {
    const char c = p[i];
    uint32_t d;
    if (c >= '0' && c <= '9')
      d = uint32_t(c - '0');
    else if (c >= 'a' && c <= 'f')
      d = uint32_t(c - 'a' + 10);
    else
      return -1;
    idx = (idx << 4) | d;
  }
  if (strcmp(p + 8, ".dat") != 0)
    return -1;
  return int64_t(idx);
}

class SegmentRange {
public:
  void add(uint32_t index) {
    if (count_ == 0) {
      oldest_ = newest_ = index;
    } else {
      if (index < oldest_)
        oldest_ = index;
      if (index > newest_)
        newest_ = index;
    }
    count_++;
  }
  bool empty() const { return count_ == 0; }
  uint16_t count() const { return count_; }
  uint32_t oldest() const { return oldest_; }
  uint32_t newest() const { return newest_; }
  uint32_t nextIndex() const { return count_ == 0 ? 0 : newest_ + 1; }
  void reset() {
    count_ = 0;
    oldest_ = newest_ = 0;
  }

private:
  uint16_t count_ = 0;
  uint32_t oldest_ = 0;
  uint32_t newest_ = 0;
};

} // namespace chronik
} // namespace ps
