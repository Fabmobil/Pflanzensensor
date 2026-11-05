/**
 * @file canvas.cpp
 */
#include "core/display/canvas.h"

#include <string.h>

#include "core/display/font5x7.h"

namespace ps {

void Canvas::clear() { memset(fb_, 0, sizeof(fb_)); }

void Canvas::pixel(int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
    return;
  uint8_t& b = fb_[(y / 8) * kWidth + x];
  const uint8_t m = static_cast<uint8_t>(1u << (y & 7));
  if (on)
    b |= m;
  else
    b &= static_cast<uint8_t>(~m);
}

bool Canvas::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight)
    return false;
  return fb_[(y / 8) * kWidth + x] & (1u << (y & 7));
}

void Canvas::hline(int x, int y, int w) {
  for (int i = 0; i < w; i++)
    pixel(x + i, y);
}

void Canvas::fillRect(int x, int y, int w, int h, bool on) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      pixel(x + i, y + j, on);
}

void Canvas::bitmap(int x, int y, const uint8_t* bmp, int w, int h) {
  const int stride = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      const uint8_t b = ps_read_byte(bmp + j * stride + i / 8);
      if (b & (0x80 >> (i & 7)))
        pixel(x + i, y + j);
    }
}

uint8_t nextGlyph(const char*& p) {
  const uint8_t c = static_cast<uint8_t>(*p);
  if (c < 0x80) {
    p++;
    if (c < font::kFirst || c > font::kLast)
      return font::kUnknown;
    return static_cast<uint8_t>(c - font::kFirst);
  }
  // Zwei-Byte-Folgen, die wir kennen
  const uint8_t d = static_cast<uint8_t>(p[1]);
  if (c == 0xC3 && d) {
    p += 2;
    switch (d) {
    case 0xA4:
      return font::kAuml;
    case 0xB6:
      return font::kOuml;
    case 0xBC:
      return font::kUuml;
    case 0x84:
      return font::kAumlC;
    case 0x96:
      return font::kOumlC;
    case 0x9C:
      return font::kUumlC;
    case 0x9F:
      return font::kSzlig;
    default:
      return font::kUnknown;
    }
  }
  if (c == 0xC2 && d) {
    p += 2;
    switch (d) {
    case 0xB0:
      return font::kDegree;
    case 0xB5:
      return 'u' - font::kFirst; // µ
    case 0xB2:
      return '2' - font::kFirst; // ²
    case 0xB3:
      return '3' - font::kFirst; // ³
    default:
      return font::kUnknown;
    }
  }
  // Andere Folgen überspringen
  p++;
  while ((static_cast<uint8_t>(*p) & 0xC0) == 0x80)
    p++;
  return font::kUnknown;
}

int Canvas::glyphCount(const char* text) {
  int n = 0;
  for (const char* p = text; p && *p;) {
    nextGlyph(p);
    n++;
  }
  return n;
}

void Canvas::glyph(int x, int y, uint8_t index, uint8_t scale) {
  if (index >= font::kCount)
    index = font::kUnknown;
  for (int col = 0; col < 5; col++) {
    const uint8_t bits = ps_read_byte(&font::kGlyphs[index][col]);
    for (int row = 0; row < 8; row++) {
      if (!(bits & (1u << row)))
        continue;
      if (scale == 1)
        pixel(x + col, y + row);
      else
        fillRect(x + col * scale, y + row * scale, scale, scale);
    }
  }
}

int Canvas::text(int x, int y, const char* t, uint8_t scale) {
  for (const char* p = t; p && *p;) {
    glyph(x, y, nextGlyph(p), scale);
    x += charAdvance(scale);
  }
  return x;
}

void Canvas::textFit(int x, int y, const char* t, int maxChars, uint8_t scale) {
  int n = 0;
  const int total = glyphCount(t);
  for (const char* p = t; p && *p;) {
    if (total > maxChars && n == maxChars - 1) {
      glyph(x, y, '~' - font::kFirst, scale);
      return;
    }
    glyph(x, y, nextGlyph(p), scale);
    x += charAdvance(scale);
    n++;
  }
}

void Canvas::textCentered(int y, const char* t, uint8_t scale) {
  const int maxChars = kWidth / charAdvance(scale);
  int n = glyphCount(t);
  if (n > maxChars)
    n = maxChars;
  const int w = n * charAdvance(scale) - scale;
  textFit((kWidth - w) / 2, y, t, maxChars, scale);
}

uint32_t Canvas::hash() const {
  uint32_t h = 2166136261u;
  for (uint8_t b : fb_) {
    h ^= b;
    h *= 16777619u;
  }
  return h;
}

} // namespace ps
