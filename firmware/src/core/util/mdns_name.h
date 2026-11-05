/**
 * @file mdns_name.h
 * @brief Hostname aus dem Gerätenamen ableiten (übernommen aus dem Altcode)
 * @details Kleinbuchstaben; ä/ö/ü/ß → ae/oe/ue/ss; andere Zeichen → "-";
 *          keine doppelten oder randständigen "-"; höchstens 63 Zeichen;
 *          leer → "pflanzensensor".
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ps {
namespace mdns {

constexpr size_t kMaxLen = 63;
constexpr const char* kFallback = "pflanzensensor";

inline const char* umlaut(unsigned char a, unsigned char b) {
  if (a != 0xC3)
    return nullptr;
  switch (b) {
  case 0xA4:
  case 0x84:
    return "ae";
  case 0xB6:
  case 0x96:
    return "oe";
  case 0xBC:
  case 0x9C:
    return "ue";
  case 0x9F:
    return "ss";
  default:
    return nullptr;
  }
}

inline size_t hostnameFrom(const char* name, char* out, size_t outSize) {
  if (!out || outSize == 0)
    return 0;
  const size_t room = (outSize - 1 < kMaxLen) ? outSize - 1 : kMaxLen;
  size_t at = 0;
  auto add = [&](char c) {
    if (c == '-' && (at == 0 || out[at - 1] == '-'))
      return;
    if (at < room)
      out[at++] = c;
  };
  for (const char* p = name ? name : ""; *p; p++) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c >= 0x80) {
      const char* rep = umlaut(c, static_cast<unsigned char>(p[1]));
      if (rep) {
        for (const char* e = rep; *e; e++)
          add(*e);
        p++;
      } else {
        add('-');
        while ((static_cast<unsigned char>(p[1]) & 0xC0) == 0x80)
          p++;
      }
      continue;
    }
    if (c >= 'A' && c <= 'Z')
      add(static_cast<char>(c - 'A' + 'a'));
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')
      add(static_cast<char>(c));
    else
      add('-');
  }
  while (at > 0 && out[at - 1] == '-')
    at--;
  out[at] = '\0';
  if (at == 0) {
    strncpy(out, kFallback, outSize - 1);
    out[outSize - 1] = '\0';
    return strlen(out);
  }
  return at;
}

} // namespace mdns
} // namespace ps
