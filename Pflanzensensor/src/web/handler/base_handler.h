/**
 * @file base_handler.h
 * @brief Base class for web request handlers
 * @details Provides comprehensive base functionality for web request handling:
 *          - Request processing
 *          - Response generation
 *          - Content management
 *          - Error handling
 *          - Resource cleanup
 */

#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include <map>
#include <string>
#include <vector>

#include "logger/logger.h"
#include "utils/result_types.h"
#include "web/core/components.h"
#include "web/core/web_router.h"

/**
 * @class BaseHandler
 * @brief Enhanced base class for web request handlers
 * @details Provides common functionality for web request handling:
 *          - Request processing
 *          - Response formatting
 *          - Content generation
 *          - Resource management
 *          - Error handling
 */
class BaseHandler {
 public:
  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @details Initializes handler with server reference
   */
  explicit BaseHandler(ESP8266WebServer& server) : _server(server) {}

  // Prevent copying
  BaseHandler(const BaseHandler&) = delete;
  BaseHandler& operator=(const BaseHandler&) = delete;

  /**
   * @brief Virtual destructor
   * @details Ensures proper cleanup of derived classes
   */
  virtual ~BaseHandler() = default;

  /**
   * @brief Clean up handler resources
   * @return true if cleanup was successful, false if already cleaned
   * @details Performs cleanup operations:
   *          - Calls onCleanup() for derived class-specific cleanup
   *          - Sets cleanup flag
   *          - Prevents multiple cleanups
   *
   * @note Derived classes should override onCleanup() for custom cleanup logic.
   */
  virtual bool cleanup() {
    if (!_cleaned) {
      onCleanup();
      _cleaned = true;
      return true;
    }
    return false;
  }

  /**
   * @brief Hook for derived classes to perform custom cleanup.
   * @details Called by cleanup() if not already cleaned. Override in derived
   * classes for custom cleanup logic.
   */
  virtual void onCleanup() {}

  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating success or failure
   * @details Pure virtual function for GET request processing
   */
  virtual HandlerResult handleGet(const String& uri,
                                  const std::map<String, String>& query) = 0;

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Handler result indicating success or failure
   * @details Pure virtual function for POST request processing
   */
  virtual HandlerResult handlePost(const String& uri,
                                   const std::map<String, String>& params) = 0;

  /**
   * @brief Register routes with router
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Calls onRegisterRoutes for derived class-specific route
   * registration. Prevents multiple registrations if needed.
   *
   * @note Derived classes should override onRegisterRoutes for custom logic.
   */
  virtual RouterResult registerRoutes(WebRouter& router) final {
    return onRegisterRoutes(router);
  }

  /**
   * @brief Hook for derived classes to register routes.
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Called by registerRoutes(). Override in derived classes for custom
   * route registration logic.
   */
  virtual RouterResult onRegisterRoutes(WebRouter& router) = 0;

 protected:
  ESP8266WebServer& _server;  ///< Protected server reference
  bool _cleaned = false;  ///< Flag indicating if handler has been cleaned up

  /**
   * @brief Render page with standard layout (DEPRECATED)
   * @param title Page title
   * @param activeNav Active navigation item
   * @param content Content generation function
   * @param additionalCss Additional CSS files
   * @param additionalScripts Additional JS files
   * @details DEPRECATED: Use renderPixelatedPage instead.
   *          Renders complete HTML page with:
   *          - Header with title
   *          - Navigation menu
   *          - Content container
   *          - Footer with version
   *          - Required scripts
   */
  void renderPage(
      const String& title, const String& activeNav,
      std::function<void()> content,
      const std::vector<String>& additionalCss = std::vector<String>(),
      const std::vector<String>& additionalScripts = std::vector<String>()) {
    if (!Component::beginResponse(_server, title, additionalCss)) {
      return;
    }

    Component::sendNavigation(_server, activeNav);
    Component::sendChunk(_server, F("<div class='main-container'>"));
    Component::sendChunk(_server, F("<div class='content-container'>"));
    Component::sendChunk(_server, F("<div class='page-container'>"));

    content();

    Component::sendChunk(_server, F("</div></div></div>"));
    Component::sendFooter(_server, VERSION, __DATE__);
    Component::endResponse(_server, additionalScripts);
  }

  /**
   * @brief Render start page with pixelated design (flower graphic, sensors)
   * @param title Page title shown in cloud
   * @param activeSection Active navigation section
   * @param content Lambda function that generates page content (flower, sensors)
   * @param additionalCss Additional CSS files to include
   * @param additionalScripts Additional JavaScript files to include
   * @param statusClass Status class for background (status-green, status-red, etc.)
   */
  void renderStartPage(
      const String& title, const String& activeSection,
      std::function<void()> content,
      const std::vector<String>& additionalCss = std::vector<String>(),
      const std::vector<String>& additionalScripts = std::vector<String>(),
      const String& statusClass = "status-unknown") {

    // Ensure start.css is included
    std::vector<String> css = {"start"};
    css.insert(css.end(), additionalCss.begin(), additionalCss.end());

    if (!Component::beginResponse(_server, title, css)) {
      return;
    }

    Component::beginPixelatedPage(_server, statusClass);
    Component::sendCloudTitle(_server, title);

    // Content goes directly into .group (flower, sensors, etc.)
    content();

    Component::sendPixelatedFooter(_server, VERSION, __DATE__, activeSection);
    Component::endPixelatedPage(_server);
    Component::endResponse(_server, additionalScripts);
  }

  /**
   * @brief Render admin/logs page with dark content box
   * @param title Page title shown in cloud
   * @param activeSection Active navigation section (admin, sensors, display, wifi, system, ota, logs)
   * @param content Lambda function that generates content inside dark box
   * @param additionalCss Additional CSS files to include
   * @param additionalScripts Additional JavaScript files to include
   */
  void renderAdminPage(
      const String& title, const String& activeSection,
      std::function<void()> content,
      const std::vector<String>& additionalCss = std::vector<String>(),
      const std::vector<String>& additionalScripts = std::vector<String>()) {

    // Ensure start.css and admin.css are included
    std::vector<String> css = {"start", "admin"};
    css.insert(css.end(), additionalCss.begin(), additionalCss.end());

    if (!Component::beginResponse(_server, title, css)) {
      return;
    }

    Component::beginPixelatedPage(_server, "status-unknown");
    Component::sendCloudTitle(_server, title);

    // Content wrapped in dark content box (90% width) with section indicator
    Component::beginContentBox(_server, activeSection);
    content();
    Component::endContentBox(_server);

    Component::sendPixelatedFooter(_server, VERSION, __DATE__, activeSection);
    Component::endPixelatedPage(_server);
    Component::endResponse(_server, additionalScripts);
  }

  /**
   * @brief DEPRECATED: Render a complete page with pixelated design
   * @deprecated Use renderStartPage() for start page or renderAdminPage() for admin/logs
   */
  void renderPixelatedPage(
      const String& title, const String& activeSection,
      std::function<void()> content,
      const std::vector<String>& additionalCss = std::vector<String>(),
      const std::vector<String>& additionalScripts = std::vector<String>(),
      const String& statusClass = "status-unknown",
      bool showContentBox = true) {

    // Ensure start.css is included
    std::vector<String> css = {"start"};
    css.insert(css.end(), additionalCss.begin(), additionalCss.end());

    if (!Component::beginResponse(_server, title, css)) {
      return;
    }

    Component::beginPixelatedPage(_server, statusClass);

    // Cloud title BEFORE content box
    Component::sendCloudTitle(_server, title);

    if (showContentBox) {
      Component::beginContentBox(_server);
      content();
      Component::endContentBox(_server);
    } else {
      content();
    }

    Component::sendPixelatedFooter(_server, VERSION, __DATE__, activeSection);
    Component::endPixelatedPage(_server);
    Component::endResponse(_server, additionalScripts);
  }

  /**
   * @brief Send JSON response
   * @param code HTTP status code
   * @param json JSON content
   * @details Sends JSON response with appropriate headers
   */
  void sendJsonResponse(int code, const String& json) {
    _server.send(code, F("application/json"), json);
  }

  /**
   * @brief Send HTML response
   * @param code HTTP status code
   * @param html HTML content
   * @details Sends HTML response with appropriate headers
   */
  void sendHtmlResponse(int code, const String& html) {
    _server.send(code, F("text/html"), html);
  }

  /**
   * @brief Send redirect response
   * @param url Redirect target URL
   * @details Sends 302 redirect with Location header
   */
  void sendRedirect(const String& url) {
    _server.sendHeader(F("Location"), url);
    _server.send(302, F("text/plain"), F(""));
  }

  /**
   * @brief Send error response
   * @param code HTTP status code
   * @param message Error message
   * @details Sends error response with plain text message
   */
  void sendError(int code, const String& message) {
    _server.send(code, F("text/plain"), message);
  }

  /**
   * @brief Get request argument
   * @param name Argument name
   * @param defaultValue Default value if argument not found
   * @return Argument value or default
   * @details Retrieves argument from request:
   *          - Checks argument existence
   *          - Returns value or default
   */
  String getArg(const String& name, const String& defaultValue = "") {
    if (!_server.hasArg(name)) {
      return defaultValue;
    }
    return _server.arg(name);
  }

  /**
   * @brief Check Content-Type header
   * @param expected Expected content type
   * @return true if content type matches
   * @details Validates request Content-Type:
   *          - Checks header existence
   *          - Compares with expected type
   */
  bool hasValidContentType(const String& expected) {
    if (!_server.hasHeader("Content-Type")) {
      return false;
    }
    return _server.header("Content-Type").equals(expected);
  }

  /**
   * @brief Begin chunked response
   * @param contentType Response content type
   * @return true if response started successfully
   * @details Sets up chunked response:
   *          - Sets unknown content length
   *          - Sets content type
   *          - Sets connection close
   *          - Sends initial response
   */
  bool beginChunkedResponse(const String& contentType) {
    static const char CONTENT_TYPE[] PROGMEM = "Content-Type";
    static const char CONNECTION[] PROGMEM = "Connection";
    static const char CLOSE[] PROGMEM = "close";

    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.sendHeader(FPSTR(CONTENT_TYPE), contentType);
    _server.sendHeader(FPSTR(CONNECTION), FPSTR(CLOSE));
    _server.send(200, contentType, F(""));
    return true;
  }

  /**
   * @brief Send response chunk
   * @param chunk Content chunk to send
   * @details Sends part of chunked response
   */
  void sendChunk(const String& chunk) { Component::sendChunk(_server, chunk); }

  /**
   * @brief End chunked response
   * @details Finalizes chunked response:
   *          - Sends empty chunk
   *          - Closes connection
   */
  void endChunkedResponse() { _server.sendContent(""); }

  /**
   * @brief Format build date
   * @param timestamp Build timestamp
   * @return Formatted date string
   * @details Formats timestamp to readable date:
   *          - Converts to GMT
   *          - Formats as "MMM DD YYYY"
   *          - Handles buffer management
   */
  String formatBuildDate(time_t timestamp) {
    char buildDate[32];
    struct tm* timeinfo = gmtime(&timestamp);
    strftime(buildDate, sizeof(buildDate), "%b %d %Y", timeinfo);
    return String(buildDate);
  }
};
