#include "display.h"

#include <ESP8266WiFi.h>
#include <LittleFS.h>
// #include <qrcodegen.h>  // QR code generation (commented out)

#include "configs/config.h"
#include "display_qrcode.h" // microqrcode
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/critical_section.h"

DisplayResult SSD1306Display::begin() {
  if (m_initialized)
    return DisplayResult::success();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!m_display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    logger.error(F("Display"), F("Display konnte nicht initialisiert werden"));
    return DisplayResult::fail(DisplayError::INITIALIZATION_ERROR,
                               F("Display konnte nicht initialisiert werden"));
  }

  m_display.clearDisplay();
  m_display.setTextColor(SSD1306_WHITE);
  m_display.display();
  m_initialized = true;
  return DisplayResult::success();
}

DisplayResult SSD1306Display::clear() {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display nicht initialisiert"));
  }

  m_display.clearDisplay();
  m_display.display();
#endif
  return DisplayResult::success();
}

String SSD1306Display::convertSpecialChars(const String& text) {
  String result = text;

  // Replace German umlauts and special characters
  result.replace("ä", "ae");
  result.replace("ö", "oe");
  result.replace("ü", "ue");
  result.replace("Ä", "Ae");
  result.replace("Ö", "Oe");
  result.replace("Ü", "Ue");
  result.replace("ß", "ss");
  result.replace("°", "*"); // Replace degree symbol with asterisk

  return result;
}

DisplayResult SSD1306Display::showText(const String& text) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  String displayText = convertSpecialChars(text);

  m_display.clearDisplay();
  m_display.setTextSize(1);
  m_display.setTextColor(SSD1306_WHITE);
  m_display.setCursor(0, 0);
  m_display.println(displayText);
  m_display.display();
#endif
  return DisplayResult::success();
}

DisplayResult SSD1306Display::showImage(const String& imagePath) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  {
    CriticalSection cs;
    if (!LittleFS.exists(imagePath)) {
      logger.error(F("Display"), String(F("Bilddatei nicht gefunden: ")) + imagePath);
      return DisplayResult::fail(DisplayError::FILE_ERROR,
                                 String(F("Bilddatei nicht gefunden: ")) + imagePath);
    }

    File imageFile = LittleFS.open(imagePath, "r");
    if (!imageFile) {
      logger.error(F("Display"), String(F("Öffnen der Bilddatei fehlgeschlagen: ")) + imagePath);
      return DisplayResult::fail(DisplayError::FILE_ERROR,
                                 String(F("Öffnen der Bilddatei fehlgeschlagen: ")) + imagePath);
    }

    m_display.clearDisplay();
    m_display.display();
    imageFile.close();
  }

  return DisplayResult::success();
#endif
  return DisplayResult::success();
}

DisplayResult SSD1306Display::showBitmap(const unsigned char* bitmap) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  m_display.clearDisplay();
  m_display.drawBitmap(0, 0, bitmap, DISPLAY_WIDTH, DISPLAY_HEIGHT, SSD1306_WHITE);
  m_display.display();
  return DisplayResult::success();
#endif
  return DisplayResult::success();
}

DisplayResult SSD1306Display::showMeasurementValue(const String& measurementName,
                                                   float measurementValue,
                                                   const String& measurementUnit) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  String displayName = convertSpecialChars(measurementName);
  String displayUnit = convertSpecialChars(measurementUnit);

  m_display.clearDisplay();

  // Draw a horizontal line at the top
  m_display.drawLine(0, 0, DISPLAY_WIDTH - 1, 0, SSD1306_WHITE);

  // Prepare value + unit string (assume unit is always 2 chars)
  char valueStr[10];
  dtostrf(measurementValue, 4, 1, valueStr);                   // e.g. " 4.4"
  String valueWithUnit = String(valueStr) + " " + displayUnit; // e.g. "4.4 %"

  // Set text size and color for value+unit

  int16_t x1, y1;
  uint16_t w, h;
  m_display.setTextColor(SSD1306_WHITE);
  // Display measurement name in smaller font below, centered
  m_display.setTextSize(1);
  m_display.getTextBounds(displayName, 0, 0, &x1, &y1, &w, &h);
  int nameX = (DISPLAY_WIDTH - w) / 2;
  if (nameX < 0)
    nameX = 0;
  m_display.setCursor(nameX, 14);
  m_display.print(displayName);

  // Center the value+unit string
  m_display.getTextBounds(valueWithUnit, 0, 0, &x1, &y1, &w, &h);
  int valueX = (DISPLAY_WIDTH - w) / 2;
  if (valueX < 0)
    valueX = 0;
  m_display.setTextSize(2);
  m_display.setCursor(valueX, 36);
  m_display.print(valueWithUnit);

  // Draw a horizontal line at the bottom
  m_display.drawLine(0, DISPLAY_HEIGHT - 1, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, SSD1306_WHITE);

  m_display.display();
#endif
  return DisplayResult::success();
}

void SSD1306Display::drawCenteredText(const String& text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  m_display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int x = (128 - w) / 2;
  m_display.setCursor(x, y);
  m_display.println(text);
}

/**
 * @brief Displays a QR code for the given text, scaled by 2x, right-aligned.
 * @param text The text or URL to encode as a QR code.
 * @return DisplayResult indicating success or failure with error details.
 */
DisplayResult SSD1306Display::showQrCode2x(const String& text) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }
  m_display.clearDisplay();
  // Try version 2 first, then version 3
  const uint8_t ecc = ECC_LOW;
  char qrtext[64];
  text.toCharArray(qrtext, sizeof(qrtext));
  uint8_t qrcodeData2[qrcode_getBufferSize(2)];
  QRCode qrcode2;
  int8_t ok2 = qrcode_initText(&qrcode2, qrcodeData2, 2, ecc, qrtext);
  if (ok2) {
    drawQrCode2x(qrcode2);
    m_display.display();
    return DisplayResult::success();
  }
  uint8_t qrcodeData3[qrcode_getBufferSize(3)];
  QRCode qrcode3;
  int8_t ok3 = qrcode_initText(&qrcode3, qrcodeData3, 3, ecc, qrtext);
  if (ok3) {
    drawQrCode2x(qrcode3);
    m_display.display();
    return DisplayResult::success();
  }
  m_display.setCursor(0, 0);
  m_display.println(F("QR ERR"));
  m_display.display();
  return DisplayResult::fail(DisplayError::INVALID_CONFIG, F("QR-Code Generierung fehlgeschlagen"));
#else
  return DisplayResult::success();
#endif
}

/**
 * @brief Draws a QR code using microqrcode, scaled by 2x, right-aligned.
 * @param qrcode The QRCode struct.
 */
void SSD1306Display::drawQrCode2x(const QRCode& qrcode) {
#if USE_DISPLAY
  int scale = 2;
  int size = qrcode.size;
  int qrX = 128 - size * scale; // right-aligned
  int qrY = 0;                  // top
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (qrcode_getModule((QRCode*)&qrcode, x, y)) {
        m_display.fillRect(qrX + x * scale, qrY + y * scale, scale, scale, SSD1306_WHITE);
      }
    }
  }
#endif
}

// Helper to truncate text to fit max width
String SSD1306Display::truncateToFit(const String& text, int maxWidth) {
  String out = text;
  int16_t x1, y1;
  uint16_t w, h;
  while (out.length() > 0) {
    m_display.getTextBounds(out, 0, 0, &x1, &y1, &w, &h);
    if (w <= maxWidth)
      break;
    out.remove(out.length() - 1);
  }
  if (out != text)
    out += F("~");
  return out;
}

DisplayResult SSD1306Display::showInfoScreen(const String& ipAddress) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  m_display.clearDisplay();
  m_display.setTextSize(1);
  m_display.setTextColor(SSD1306_WHITE);

  // Draw top and bottom lines
  m_display.drawLine(0, 0, 127, 0, SSD1306_WHITE);
  m_display.drawLine(0, 63, 127, 63, SSD1306_WHITE);

  // Prepare info
  String name = ConfigMgr.getDeviceName();
  String ip = ipAddress;
  if (ip.startsWith(F("http://")))
    ip = ip.substring(7);
  String versionStr = "v" VERSION;
  String ssid;
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    ssid = WiFi.softAPSSID();
    if (ssid.length() == 0)
      ssid = String(F("(AP SSID unbekannt)"));
  } else {
    ssid = WiFi.SSID();
    if (ssid.length() == 0)
      ssid = String(F("(SSID unbekannt)"));
  }

  // Build URL for QR code
  String url = F("http://");
  url += ip;
  url.trim();

  // Update cached QR code if needed
  bool qrValid = updateQrCodeIfNeeded(url);
  logger.debug(F("DisplayM"), F("QR input: ") + url + F(" (len=") + String(url.length()) + F(")"));

  // Layout
  int qrScale = 2;
  int qrSize = (qrValid) ? (m_cachedQrcode.size * qrScale) : 0;
  int textBlockWidth = 128 - qrSize - 4; // 4px margin
  if (textBlockWidth < 40)
    textBlockWidth = 40; // minimum for text
  int yOffset = 8;

  // Draw stacked text block (left side), truncating if needed
  m_display.setCursor(0, yOffset);
  m_display.println(truncateToFit(name, textBlockWidth));
  m_display.setCursor(0, yOffset + 12);
  m_display.println(truncateToFit(versionStr, textBlockWidth));
  m_display.setCursor(0, yOffset + 24);
  m_display.println(truncateToFit(ip, textBlockWidth));
  m_display.setCursor(0, yOffset + 36);
  m_display.println(truncateToFit(ssid, textBlockWidth));

  // Draw QR code (right-aligned)
  if (qrValid) {
    int qrX = 128 - qrSize;
    int qrY = (64 - qrSize) / 2;
    for (int y = 0; y < m_cachedQrcode.size; ++y) {
      for (int x = 0; x < m_cachedQrcode.size; ++x) {
        if (qrcode_getModule(&m_cachedQrcode, x, y)) {
          m_display.fillRect(qrX + x * qrScale, qrY + y * qrScale, qrScale, qrScale, SSD1306_WHITE);
        }
      }
    }
  } else {
    m_display.setCursor(0, 56);
    m_display.println(F("QR ERR"));
  }

  m_display.display();
#endif
  return DisplayResult::success();
}

bool SSD1306Display::updateQrCodeIfNeeded(const String& url) {
  // Check if URL has changed
  if (m_qrcodeValid && m_lastQrUrl == url) {
    return true; // Use cached QR code
  }

  // URL changed, regenerate QR code
  const uint8_t ecc = ECC_LOW;
  char qrtext[64];
  url.toCharArray(qrtext, sizeof(qrtext));

  // Try version 2 first, then version 3
  int8_t ok2 = qrcode_initText(&m_cachedQrcode, m_qrcodeData2, 2, ecc, qrtext);
  if (ok2) {
    m_cachedQrVersion = 2;
    m_qrcodeValid = true;
    m_lastQrUrl = url;
    logger.debug(F("DisplayM"), F("QR code cached (v2) for: ") + url);
    return true;
  }

  int8_t ok3 = qrcode_initText(&m_cachedQrcode, m_qrcodeData3, 3, ecc, qrtext);
  if (ok3) {
    m_cachedQrVersion = 3;
    m_qrcodeValid = true;
    m_lastQrUrl = url;
    logger.debug(F("DisplayM"), F("QR code cached (v3) for: ") + url);
    return true;
  }

  // Failed to generate QR code
  m_qrcodeValid = false;
  m_lastQrUrl = "";
  logger.debug(F("DisplayM"), F("QR code generation failed for: ") + url);
  return false;
}

DisplayResult SSD1306Display::showClock(const String& dateStr, const String& timeStr) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  m_display.clearDisplay();

  // Draw a horizontal line at the top
  m_display.drawLine(0, 0, DISPLAY_WIDTH - 1, 0, SSD1306_WHITE);

  // Display time in large font
  m_display.setTextSize(3);
  m_display.setTextColor(SSD1306_WHITE);

  // Center the time string
  int16_t x1, y1;
  uint16_t w, h;
  m_display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  int timeX = (DISPLAY_WIDTH - w) / 2;
  m_display.setCursor(timeX, 16);
  m_display.print(timeStr);

  // Display date in smaller font below
  m_display.setTextSize(1);
  m_display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
  int dateX = (DISPLAY_WIDTH - w) / 2;
  m_display.setCursor(dateX, 48);
  m_display.print(dateStr);

  // Draw a horizontal line at the bottom
  m_display.drawLine(0, DISPLAY_HEIGHT - 1, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, SSD1306_WHITE);

  m_display.display();
#endif
  return DisplayResult::success();
}

DisplayResult SSD1306Display::showBootScreen(const String& header,
                                             const std::vector<String>& lines) {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }
  m_display.clearDisplay();
  m_display.setTextSize(1);
  m_display.setTextColor(SSD1306_WHITE);
  m_display.setCursor(0, 0);
  m_display.println(convertSpecialChars(header));
  int y = 16; // Start below header
  for (const auto& line : lines) {
    m_display.setCursor(0, y);
    m_display.println(convertSpecialChars(line));
    y += 8; // 8px per line at text size 1
    if (y > DISPLAY_HEIGHT - 8)
      break;
  }
  m_display.display();
#endif
  return DisplayResult::success();
}

DisplayResult SSD1306Display::showBootScreen(const String& header, const String& status) {
  return showBootScreen(header, std::vector<String>{status});
}

DisplayResult SSD1306Display::switchDisplay(bool enabled) {
#if USE_DISPLAY
  if (!m_initialized && enabled) {
    auto result = begin();
    if (!result.isSuccess()) {
      return result;
    }
  }
  if (enabled) {
    m_display.display();
  } else {
    m_display.clearDisplay();
    m_display.display();
  }
  m_initialized = enabled;
#endif
  return DisplayResult::success();
}
