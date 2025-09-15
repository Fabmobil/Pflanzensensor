/**
 * @file display.h
 * @brief Header file for SSD1306 OLED display functionality
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>

// #include <qrcodegen.h>  // QR code generation (commented out)
#include "../configs/config.h"
#include "../logger/logger.h"
#include "../utils/result_types.h"
#include "display_qrcode.h"  // microqrcode

/**
 * @struct SSD1306DisplayStatus
 * @brief Status information for the SSD1306 display
 */
struct SSD1306DisplayStatus {
  String activeScreen;
  bool active;
};

/**
 * @class SSD1306Display
 * @brief Controls a SSD1306 OLED display
 */
class SSD1306Display {
 public:
  SSD1306Display()
      : m_display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, DISPLAY_RESET),
        m_initialized(false) {}
  ~SSD1306Display() = default;

  /**
   * @brief Initializes the SSD1306 display.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult begin();

  /**
   * @brief Clears the display.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult clear();

  /**
   * @brief Displays the given text on the screen.
   * @param text The text to display.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showText(const String& text);

  /**
   * @brief Displays an image from the specified path.
   * @param imagePath The path to the image file.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showImage(const String& imagePath);

  /**
   * @brief Displays a bitmap image from memory.
   * @param bitmap Pointer to the bitmap data.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showBitmap(const unsigned char* bitmap);

  /**
   * @brief Displays a measurement value with its name and unit.
   * @param measurementName The name of the measurement.
   * @param measurementValue The value of the measurement.
   * @param measurementUnit The unit of the measurement.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showMeasurementValue(const String& measurementName,
                                     float measurementValue,
                                     const String& measurementUnit);

  /**
   * @brief Displays the given IP address on the screen.
   * @param ipAddress The IP address to display.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showInfoScreen(const String& ipAddress);

  /**
   * @brief Displays the current time and date on the screen.
   * @param dateStr The date string to display.
   * @param timeStr The time string to display.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showClock(const String& dateStr, const String& timeStr);

  /**
   * @brief Switches the display on or off.
   * @param enabled True to turn on the display, false to turn it off.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult switchDisplay(bool enabled);

  /**
   * @brief Displays a boot screen with a header and multiple status lines.
   * @param header The header text (e.g., device name and 'startet:').
   * @param lines The status lines to display under the header.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showBootScreen(const String& header,
                               const std::vector<String>& lines);

  /**
   * @brief Displays a boot screen with a header and a status line
   * (compatibility).
   * @param header The header text.
   * @param status The status line.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showBootScreen(const String& header, const String& status);

  /**
   * @brief Displays a QR code for the given text, scaled by 2x, right-aligned.
   * @param text The text or URL to encode as a QR code.
   * @return DisplayResult indicating success or failure with error details.
   */
  DisplayResult showQrCode2x(const String& text);

 private:
  /**
   * @brief Converts German umlauts and special characters to ASCII equivalents.
   * @param text The text to convert.
   * @return The converted text with ASCII equivalents.
   */
  String convertSpecialChars(const String& text);

  /**
   * @brief Truncates a string to fit within a given pixel width, adding
   * ellipsis if needed.
   * @param text The text to truncate.
   * @param maxWidth The maximum pixel width.
   * @return The truncated string.
   */
  String truncateToFit(const String& text, int maxWidth);

  /**
   * @brief Draws text centered horizontally at the given y position.
   * @param text The text to draw.
   * @param y The y position (row) to draw the text.
   */
  void drawCenteredText(const String& text, int y);

  /**
   * @brief Draws a QR code using microqrcode, scaled by 2x, right-aligned.
   * @param qrcode The QRCode struct.
   */
  void drawQrCode2x(const QRCode& qrcode);

  /**
   * @brief Updates the cached QR code if the IP address has changed.
   * @param url The URL to encode as a QR code.
   * @return true if QR code was generated successfully, false otherwise.
   */
  bool updateQrCodeIfNeeded(const String& url);

  // Cached QR code data
  String m_lastQrUrl;
  uint8_t m_qrcodeData2[45];   // Fixed size for version 2:
                               // qrcode_getBufferSize(2) = 45
  uint8_t m_qrcodeData3[102];  // Fixed size for version 3:
                               // qrcode_getBufferSize(3) = 102
  QRCode m_cachedQrcode;
  bool m_qrcodeValid = false;
  uint8_t m_cachedQrVersion = 0;

  Adafruit_SSD1306 m_display;
  bool m_initialized;
};

#endif  // DISPLAY_H
