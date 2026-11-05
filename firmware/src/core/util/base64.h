/**
 * @file base64.h
 * @brief Base64 (RFC 4648) ohne Heap, auch stückweise
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/json_writer.h"

namespace ps {
namespace base64 {

inline size_t encodedLength(size_t n) { return ((n + 2) / 3) * 4; }

inline char encChar(uint8_t v) {
  v &= 0x3F;
  if (v < 26)
    return static_cast<char>('A' + v);
  if (v < 52)
    return static_cast<char>('a' + v - 26);
  if (v < 62)
    return static_cast<char>('0' + v - 52);
  return v == 62 ? '+' : '/';
}

inline int decChar(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

/// In einen Puffer kodieren (nullterminiert). @return Länge oder 0 bei zu kleinem Puffer
inline size_t encode(const uint8_t* in, size_t n, char* out, size_t cap) {
  const size_t need = encodedLength(n);
  if (cap < need + 1)
    return 0;
  size_t o = 0;
  size_t i = 0;
  for (; i + 3 <= n; i += 3) {
    const uint32_t v = (uint32_t(in[i]) << 16) | (uint32_t(in[i + 1]) << 8) | in[i + 2];
    out[o++] = encChar(uint8_t(v >> 18));
    out[o++] = encChar(uint8_t(v >> 12));
    out[o++] = encChar(uint8_t(v >> 6));
    out[o++] = encChar(uint8_t(v));
  }
  if (n - i == 1) {
    const uint32_t v = uint32_t(in[i]) << 16;
    out[o++] = encChar(uint8_t(v >> 18));
    out[o++] = encChar(uint8_t(v >> 12));
    out[o++] = '=';
    out[o++] = '=';
  } else if (n - i == 2) {
    const uint32_t v = (uint32_t(in[i]) << 16) | (uint32_t(in[i + 1]) << 8);
    out[o++] = encChar(uint8_t(v >> 18));
    out[o++] = encChar(uint8_t(v >> 12));
    out[o++] = encChar(uint8_t(v >> 6));
    out[o++] = '=';
  }
  out[o] = '\0';
  return o;
}

/// Kodiert direkt in einen Sink (in Stücken von 48 Byte).
inline void encodeTo(Sink& sink, const uint8_t* in, size_t n) {
  char buf[65];
  while (n > 0) {
    const size_t take = n > 48 ? 48 : n;
    const size_t len = encode(in, take, buf, sizeof(buf));
    sink.write(buf, len);
    in += take;
    n -= take;
  }
}

/**
 * @brief Dekodieren
 * @return Anzahl Bytes oder -1 bei ungültigem Zeichen oder zu kleinem Puffer
 */
inline int decode(const char* in, size_t len, uint8_t* out, size_t cap) {
  while (len > 0 &&
         (in[len - 1] == '=' || in[len - 1] == '\n' || in[len - 1] == '\r' || in[len - 1] == ' '))
    len--;
  size_t o = 0;
  uint32_t buf = 0;
  int bits = 0;
  for (size_t i = 0; i < len; i++) {
    const int v = decChar(in[i]);
    if (v < 0)
      return -1;
    buf = (buf << 6) | uint32_t(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (o >= cap)
        return -1;
      out[o++] = uint8_t((buf >> bits) & 0xFF);
    }
  }
  return static_cast<int>(o);
}

} // namespace base64
} // namespace ps
