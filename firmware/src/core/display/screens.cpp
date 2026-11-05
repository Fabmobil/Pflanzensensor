/**
 * @file screens.cpp
 */
#include "core/display/screens.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/display/images.h"
#include "qrcode.h"

namespace ps {
namespace screens {

void info(Canvas& c, const char* host, const char* ip, const char* ssid) {
  c.clear();
  char name[80];
  snprintf(name, sizeof(name), "%s.local", host);
  const int perLine = Canvas::kWidth / Canvas::charAdvance(1); // 21
  // Bis zu zwei Zeilen für den Namen
  const int n = Canvas::glyphCount(name);
  if (n <= perLine) {
    c.text(0, 4, name);
  } else {
    // Byte-Position des 21. Zeichens finden
    const char* p = name;
    for (int i = 0; i < perLine && *p; i++)
      nextGlyph(p);
    char first[80];
    const size_t len = static_cast<size_t>(p - name);
    memcpy(first, name, len);
    first[len] = '\0';
    c.text(0, 0, first);
    c.textFit(0, 10, p, perLine);
  }
  c.hline(0, 22, Canvas::kWidth);
  c.text(0, 28, "IP:");
  c.textFit(24, 28, ip, perLine - 4);
  c.text(0, 42, "WLAN:");
  c.textFit(36, 42, ssid, perLine - 6);
}

void clock(Canvas& c, uint8_t hour, uint8_t minute, uint8_t day, uint8_t month, uint16_t year,
           const char* ampm) {
  c.clear();
  char t[8];
  snprintf(t, sizeof(t), "%02u:%02u", hour, minute);
  c.textCentered(8, t, 4);
  if (ampm)
    c.text(Canvas::kWidth - 12, 0, ampm);
  char d[16];
  snprintf(d, sizeof(d), "%02u.%02u.%04u", day, month, year);
  c.textCentered(48, d, 1);
}

bool qr(Canvas& c, const char* url) {
  c.clear();
  c.text(0, 0, "QR zu Website:");
  QRCode code;
  // Version 3 (29×29) passt mit Skalierung 2 genau unter den Titel
  uint8_t buf[106]; // qrcode_getBufferSize(3)
  // Kapazität im Byte-Modus, ECC low: Version 2 = 32, Version 3 = 53 Zeichen.
  // Die Bibliothek prüft das nicht selbst und schreibt sonst über den Puffer
  // hinaus (vom Fuzz-Test gefunden).
  const size_t len = strlen(url);
  if (len > 53)
    return false;
  const uint8_t version = len > 32 ? 3 : 2;
  if (qrcode_initText(&code, buf, version, ECC_LOW, url) != 0)
    return false;
  const int scale = 2;
  const int size = code.size * scale;
  const int x0 = (Canvas::kWidth - size) / 2;
  const int y0 = Canvas::kHeight - size; // unten bündig
  for (uint8_t y = 0; y < code.size; y++)
    for (uint8_t x = 0; x < code.size; x++)
      if (qrcode_getModule(&code, x, y))
        c.fillRect(x0 + x * scale, y0 + y * scale, scale, scale);
  return true;
}

void flower(Canvas& c) {
  c.clear();
  c.bitmap(0, 0, images::kFlower, 128, 64);
}

void fabmobil(Canvas& c) {
  c.clear();
  c.bitmap(0, 0, images::kFabmobil, 128, 64);
}

void channel(Canvas& c, const char* name, float value, const char* unit) {
  c.clear();
  c.textCentered(2, name, 1);
  c.hline(0, 13, Canvas::kWidth);
  char v[24];
  if (isnan(value) || isinf(value))
    snprintf(v, sizeof(v), "--");
  else
    snprintf(v, sizeof(v), "%.1f%s", static_cast<double>(value), unit);
  // Größte Schrift, die passt
  uint8_t scale = 3;
  while (scale > 1 && Canvas::textWidth(v, scale) > Canvas::kWidth)
    scale--;
  c.textCentered(scale == 3 ? 26 : 30, v, scale);
  c.hline(0, 54, Canvas::kWidth);
}

void bootLog(Canvas& c, const char* header, const char* const* lines, uint8_t n) {
  c.clear();
  c.textFit(0, 0, header, 21);
  c.hline(0, 9, Canvas::kWidth);
  for (uint8_t i = 0; i < n && i < 6; i++)
    c.textFit(0, 12 + i * 9, lines[i], 21);
}

void progress(Canvas& c, const char* title, uint8_t percent) {
  c.clear();
  c.textCentered(8, title, 1);
  if (percent > 100)
    percent = 100;
  c.fillRect(4, 28, 120, 12, false);
  c.hline(4, 28, 120);
  c.hline(4, 39, 120);
  for (int y = 28; y < 40; y++) {
    c.pixel(4, y);
    c.pixel(123, y);
  }
  c.fillRect(6, 30, 116 * percent / 100, 8);
  char p[8];
  snprintf(p, sizeof(p), "%u%%", percent);
  c.textCentered(48, p, 1);
}

} // namespace screens
} // namespace ps
