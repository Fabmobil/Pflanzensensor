#include "display.h"

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <LittleFS.h>
// #include <qrcodegen.h>  // QR code generation (commented out)

#include "configs/config.h"
#include "display_qrcode.h" // microqrcode
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/critical_section.h"
#include "utils/mdns_name.h"
#include "utils/wifi.h"

DisplayResult SSD1306Display::begin() {
  if (m_initialized)
    return DisplayResult::success();

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!m_display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    LOG_ERROR(F("Display"), F("Display konnte nicht initialisiert werden"));
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
  m_display.setCursor(nameX, 16);
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

/**
 * @brief Displays a QR code for the given text, scaled by 2x, right-aligned.
 * @param text The text or URL to encode as a QR code.
 * @return DisplayResult indicating success or failure with error details.
 */
/**
 * @brief Draws a QR code using microqrcode, scaled by 2x, right-aligned.
 * @param qrcode The QRCode struct.
 */
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

/**
 * @brief Text auf zwei Zeilen aufteilen, statt ihn abzuschneiden
 * @param text Der volle Text
 * @param maxWidth Breite einer Zeile in Pixeln
 * @param zweite Nimmt den Rest auf - leer, wenn alles in eine Zeile passt
 * @return Die erste Zeile
 * @details Für den mDNS-Namen: "frameclaw-ps.local" passt gerade so, ein
 *          längerer Gerätename nicht mehr. Abschneiden wäre hier besonders
 *          ärgerlich - man kann den Namen dann nicht abtippen, und genau dafür
 *          steht er auf dem Display.
 */
String SSD1306Display::splitToFit(const String& text, int maxWidth, String& zweite) {
  zweite = "";
  int16_t x1, y1;
  uint16_t w, h;
  m_display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  if (w <= maxWidth) {
    return text;
  }
  // Größte Länge suchen, die noch passt
  size_t passt = text.length();
  while (passt > 0) {
    String versuch = text.substring(0, passt);
    m_display.getTextBounds(versuch, 0, 0, &x1, &y1, &w, &h);
    if (w <= maxWidth) {
      break;
    }
    passt--;
  }
  zweite = truncateToFit(text.substring(passt), maxWidth);
  return text.substring(0, passt);
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
  // Der mDNS-Name statt Gerätename und Version: unter dem hier abgelesenen
  // Namen ist das Gerät auch erreichbar, die Version steht im Webinterface.
  // Passt er nicht in eine Zeile, läuft er in die zweite - abtippen können
  // muss man ihn.
  String mdns = mdnsName();
  if (mdns.length() == 0) {
    char host[MdnsName::MAX_LEN + 1];
    MdnsName::hostnameVon(ConfigMgr.getDeviceName().c_str(), host, sizeof(host));
    mdns = host;
  }
  mdns += F(".local");

  String ip = ipAddress;
  if (ip.startsWith(F("http://")))
    ip = ip.substring(7);
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

  // Layout - full width for text (no QR code on this screen anymore)
  int textBlockWidth = 128;
  int yOffset = 8;

  // Draw stacked text block, truncating if needed
  String mdnsZeile2;
  const String mdnsZeile1 = splitToFit(mdns, textBlockWidth, mdnsZeile2);
  m_display.setCursor(0, yOffset);
  m_display.println(mdnsZeile1);
  m_display.setCursor(0, yOffset + 12);
  m_display.println(mdnsZeile2); // leer, wenn der Name in eine Zeile passt
  m_display.setCursor(0, yOffset + 24);
  m_display.println(truncateToFit(ip, textBlockWidth));
  m_display.setCursor(0, yOffset + 36);
  m_display.println(truncateToFit(ssid, textBlockWidth));

  m_display.display();
#endif
  return DisplayResult::success();
}

DisplayResult SSD1306Display::showQrCodeScreen() {
#if USE_DISPLAY
  if (!m_initialized) {
    return DisplayResult::fail(DisplayError::INVALID_STATE, F("Display not initialized"));
  }

  m_display.clearDisplay();

  // Prepare URL for QR code
  String ip;
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    ip = WiFi.softAPIP().toString();
  } else if (WiFi.status() == WL_CONNECTED) {
    ip = WiFi.localIP().toString();
  } else {
    ip = "0.0.0.0";
  }

  String url = F("http://");
  url += ip;
  url.trim();

  // Generate QR code
  bool qrValid = updateQrCodeIfNeeded(url);

  if (qrValid) {
    // Display title centered at top
    m_display.setTextSize(1);
    m_display.setTextColor(SSD1306_WHITE);
    String title = F("QR zu Website:");
    int16_t x1, y1;
    uint16_t w, h;
    m_display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int titleX = (128 - w) / 2;
    m_display.setCursor(titleX, 0);
    m_display.println(title);

    // Display QR code centered at bottom in 2x scale
    int scale = 2;
    int qrSize = m_cachedQrcode.size * scale;
    int qrX = (128 - qrSize) / 2;
    int qrY = 64 - qrSize; // Bottom aligned

    for (int y = 0; y < m_cachedQrcode.size; ++y) {
      for (int x = 0; x < m_cachedQrcode.size; ++x) {
        if (qrcode_getModule(&m_cachedQrcode, x, y)) {
          m_display.fillRect(qrX + x * scale, qrY + y * scale, scale, scale, SSD1306_WHITE);
        }
      }
    }
  } else {
    // QR code generation failed
    m_display.setTextSize(2);
    m_display.setTextColor(SSD1306_WHITE);
    m_display.setCursor(0, 0);
    m_display.println(F("QR-Code"));
    m_display.println(F("Fehler"));
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
    LOG_DEBUG(F("DisplayM"), String(F("QR code cached (v2) for: ")) + url);
    return true;
  }

  int8_t ok3 = qrcode_initText(&m_cachedQrcode, m_qrcodeData3, 3, ecc, qrtext);
  if (ok3) {
    m_cachedQrVersion = 3;
    m_qrcodeValid = true;
    m_lastQrUrl = url;
    LOG_DEBUG(F("DisplayM"), String(F("QR code cached (v3) for: ")) + url);
    return true;
  }

  // Failed to generate QR code
  m_qrcodeValid = false;
  m_lastQrUrl = "";
  LOG_DEBUG(F("DisplayM"), String(F("QR code generation failed for: ")) + url);
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
