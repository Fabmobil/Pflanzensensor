/**
 * @file canvas.h
 * @brief 128×64-Framebuffer im SSD1306-Seitenformat mit Text und Bitmaps
 * @details Arduino-frei, damit die Bildschirme nativ testbar sind
 *          (Pixel-Hash, refactor.md 5.14).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {

class Canvas {
public:
  static constexpr uint8_t kWidth = 128;
  static constexpr uint8_t kHeight = 64;
  static constexpr size_t kBytes = kWidth * kHeight / 8;

  void clear();
  void pixel(int x, int y, bool on = true);
  bool get(int x, int y) const;
  void hline(int x, int y, int w);
  void fillRect(int x, int y, int w, int h, bool on = true);
  /// Bitmap zeilenweise, MSB links (Flash)
  void bitmap(int x, int y, const uint8_t* bmpP, int w, int h);

  /// Zeichenbreite inkl. Abstand
  static int charAdvance(uint8_t scale) { return 6 * scale; }
  /// UTF-8-Text: Anzahl darstellbarer Zeichen
  static int glyphCount(const char* text);
  static int textWidth(const char* text, uint8_t scale) {
    return glyphCount(text) * charAdvance(scale);
  }
  /// Text zeichnen; gibt das x hinter dem Text zurück
  int text(int x, int y, const char* text, uint8_t scale = 1);
  /// Zentriert; zu lange Texte werden mit "~" gekürzt
  void textCentered(int y, const char* text, uint8_t scale = 1);
  /// Linksbündig, höchstens maxChars Zeichen, sonst "~" am Ende
  void textFit(int x, int y, const char* text, int maxChars, uint8_t scale = 1);

  const uint8_t* data() const { return fb_; }
  /// FNV-1a über den Framebuffer (Tests)
  uint32_t hash() const;

private:
  void glyph(int x, int y, uint8_t index, uint8_t scale);
  uint8_t fb_[kBytes] = {};
};

/// Nächstes UTF-8-Zeichen als Glyph-Index; rückt p weiter
uint8_t nextGlyph(const char*& p);

} // namespace ps
