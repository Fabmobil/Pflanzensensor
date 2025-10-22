/**
 * @file components.h
 * @brief Declarations for web UI components and HTML utilities
 * @details Provides a collection of reusable web UI components and utilities
 *          for building consistent HTML pages across the web interface.
 *          Includes memory-safe HTML generation and common UI elements.
 */

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include <vector>

#include "utils/result_types.h"

/**
 * @namespace Component
 * @brief Contains web UI components and utilities
 * @details Provides a collection of functions for generating HTML components
 *          and managing web page structure. Includes memory-safe operations
 *          and consistent styling across the interface.
 */
namespace Component {

// Constants
/// Minimum required heap size for safe HTML generation
static const size_t MIN_HEAP_SIZE = 3072;
/// Recommended heap size for optimal performance
static const size_t SAFE_HEAP_SIZE = 4096;

/**
 * @brief Initialize HTML response with proper headers
 * @param server Reference to web server
 * @param title Page title
 * @param additionalCss Optional vector of additional CSS files to include
 * @return ResourceResult indicating success or failure with error details
 * @details Sets up the HTML document structure with:
 *          - Proper DOCTYPE and meta tags
 *          - Title and character encoding
 *          - Base CSS and any additional stylesheets
 *          - Responsive viewport settings
 *          Performs memory checks before proceeding.
 */
ResourceResult beginResponse(ESP8266WebServer& server, const String& title,
                             const std::vector<String>& additionalCss = std::vector<String>());

/**
 * @brief Send a chunk of HTML safely with memory checks
 * @param server Reference to web server
 * @param chunk Content to send (can be String or PROGMEM string)
 * @details Safely sends HTML content in chunks to prevent memory issues:
 *          - Checks available memory before sending
 *          - Handles both RAM and PROGMEM strings
 *          - Ensures proper content encoding
 */
void sendChunk(ESP8266WebServer& server, const String& chunk);

/**
 * @brief Send pixelated footer with navigation and system info
 * @param server Reference to web server
 * @param version Version string
 * @param buildDate Build date string
 * @param activeSection Currently active section (start, logs, admin, admin/sensors, etc.)
 * @details Generates the pixelated footer containing:
 *          - Navigation links (START, LOGS, ADMIN)
 *          - Admin submenu or system stats depending on section
 *          - Version and build information
 *          - Fabmobil logo
 */
void sendPixelatedFooter(ESP8266WebServer& server, const String& version, const String& buildDate,
                         const String& activeSection);

/**
 * @brief Begin pixelated page layout
 * @param server Reference to web server
 * @param statusClass Status class for the box (status-green, status-red, etc.)
 * @details Starts the main pixelated container with status-based background
 */
void beginPixelatedPage(ESP8266WebServer& server, const String& statusClass = "status-unknown");

/**
 * @brief Send cloud title section
 * @param server Reference to web server
 * @param title Title text to display in cloud
 * @details Generates cloud image with centered title text
 */
void sendCloudTitle(ESP8266WebServer& server, const String& title);

/**
 * @brief Begin content box (dark container for cards/content)
 * @param server Reference to web server
 * @param section Optional section name for styling (admin, sensors, display, wifi, system, ota, logs)
 * @details Starts the dark content container that replaces the flower
 */
void beginContentBox(ESP8266WebServer& server, const String& section = "");

/**
 * @brief End content box
 * @param server Reference to web server
 * @details Closes the content container
 */
void endContentBox(ESP8266WebServer& server);

/**
 * @brief End pixelated page layout
 * @param server Reference to web server
 * @details Closes the main pixelated container
 */
void endPixelatedPage(ESP8266WebServer& server);

/**
 * @brief Get a display-friendly IP address string
 * @return SoftAP IP when in AP mode, otherwise station local IP or a placeholder
 */
String getDisplayIP();
/**
 * @brief Get the SSID to display in the web UI
 * @return SoftAP SSID when in AP mode, otherwise station SSID or placeholder
 */
String getDisplaySSID();

/**
 * @brief End HTML response
 * @param server Reference to web server
 * @param additionalScripts Optional vector of JavaScript files to include
 * @details Completes the HTML document:
 *          - Adds closing tags
 *          - Includes JavaScript files
 *          - Adds any final scripts
 *          - Flushes remaining content
 */
void endResponse(ESP8266WebServer& server,
                 const std::vector<String>& additionalScripts = std::vector<String>());

/**
 * @brief Create a form group component
 * @param server WebServer reference
 * @param label Label text
 * @param content Form content
 * @details Generates a styled form group containing:
 *          - Label element
 *          - Input content
 *          - Proper spacing and alignment
 *          - Accessibility attributes
 */
void formGroup(ESP8266WebServer& server, const String& label, const String& content);

/**
 * @brief Create a button component
 * @param server WebServer reference
 * @param text Button text
 * @param type Button type (button, submit)
 * @param className CSS class names
 * @param disabled Whether button is disabled
 * @param id Optional button ID
 * @details Generates a styled button element with:
 *          - Specified type and text
 *          - Custom classes for styling
 *          - Disabled state handling
 *          - Optional ID for JavaScript interaction
 *          - Proper ARIA attributes
 */
void button(ESP8266WebServer& server, const String& text, const String& type,
            const String& className = "", bool disabled = false, const String& id = "");

} // namespace Component

#endif // COMPONENTS_H
