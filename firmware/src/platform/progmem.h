/**
 * @file progmem.h
 * @brief Konstante Texte im Flash statt im RAM
 * @details Auf dem ESP8266 landen String-Literale sonst im knappen DRAM
 *          (.rodata). PS_PSTR legt sie in den Flash; gelesen wird dann nur noch
 *          über die *_P-Funktionen. Auf ESP32 und nativ ist alles normaler
 *          Speicher, die Makros fallen dort auf die Standardfunktionen zurück.
 *
 *          Einzige Stelle mit Plattform-#ifdefs, die der Kern einbindet.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <pgmspace.h>
#define PS_PROGMEM PROGMEM
#define PS_PSTR(s) PSTR(s)
#define ps_read_byte(p) pgm_read_byte(p)
#define ps_strlen_P(p) strlen_P(p)
#define ps_strcmp_P(a, p) strcmp_P(a, p)
#define ps_strncmp_P(a, p, n) strncmp_P(a, p, n)
#define ps_memcpy_P(d, p, n) memcpy_P(d, p, n)
#define ps_vsnprintf_P(b, n, f, a) vsnprintf_P(b, n, f, a)
#else
#define PS_PROGMEM
#define PS_PSTR(s) (s)
#define ps_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#define ps_strlen_P(p) strlen(p)
#define ps_strcmp_P(a, p) strcmp(a, p)
#define ps_strncmp_P(a, p, n) strncmp(a, p, n)
#define ps_memcpy_P(d, p, n) memcpy(d, p, n)
#define ps_vsnprintf_P(b, n, f, a) vsnprintf(b, n, f, a)
#endif

namespace ps {

/// Zeiger auf einen Text im Flash. Eigener Typ, damit Überladungen RAM- und
/// Flash-Texte nicht verwechseln.
struct FStr {
  const char* p;
};

/// Kopiert einen Flash-Text in einen RAM-Puffer (immer nullterminiert).
inline size_t copyP(char* dst, size_t dstSize, FStr s) {
  if (!dst || dstSize == 0)
    return 0;
  size_t i = 0;
  if (s.p) {
    for (; i + 1 < dstSize; i++) {
      const char c = static_cast<char>(ps_read_byte(s.p + i));
      if (!c)
        break;
      dst[i] = c;
    }
  }
  dst[i] = '\0';
  return i;
}

} // namespace ps

/// Flash-Text als FStr.
#define FS_(s) (::ps::FStr{PS_PSTR(s)})
