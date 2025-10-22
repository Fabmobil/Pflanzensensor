/**
 * @file web_manager.h
 * @brief Central manager for all web functionality
 * @details Provides a singleton interface for managing web server operations,
 *          including routing, authentication, and various web services.
 *          Handles initialization, request processing, and resource management
 *          for the entire web server infrastructure.
 */

#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <ArduinoJson.h>
#include <ESP8266WebServer.h>

#include <memory>
#include <vector>

#include "managers/manager_sensor.h"
#include "utils/result_types.h"
#include "web/core/web_auth.h"
#include "web/core/web_router.h"
#include "web/handler/admin_display_handler.h"
#include "web/handler/admin_handler.h"
#include "web/handler/admin_minimal_handler.h"
#include "web/handler/admin_sensor_handler.h"
#include "web/handler/log_handler.h"
#include "web/handler/sensor_handler.h"
#include "web/handler/startpage_handler.h"
#include "web/handler/web_ota_handler.h"
#include "web/handler/wifi_setup_handler.h"
#include "web/services/css_service.h"

class WiFiSetupHandler; ///< Forward declaration for WiFi setup handler

/**
 * @class WebManager
 * @brief Manages all web server functionality and related services
 * @details Implements a singleton pattern to provide centralized control over
 *          the web server, routing, authentication, and various handlers.
 *          Responsible for:
 *          - Web server initialization and shutdown
 *          - Request routing and handling
 *          - Handler lifecycle management
 *          - Authentication and security
 *          - OTA update coordination
 */
class WebManager {
public:
  /**
   * @brief Get singleton instance
   * @return Reference to WebManager instance
   */
  static WebManager& getInstance();

  /**
   * @brief Initialize web server
   * @param port Server port (default 80)
   * @return ResourceResult indicating success or failure with error details
   */
  ResourceResult begin(uint16_t port = 80);

  /**
   * @brief Set sensor manager reference
   * @param sensorManager Reference to sensor manager instance
   */
  void setSensorManager(SensorManager& sensorManager) { _sensorManager = &sensorManager; }

  /**
   * @brief Get sensor manager reference
   * @return Reference to sensor manager
   */
  SensorManager& getSensorManager() { return *_sensorManager; }

  /**
   * @brief Set firmware upgrade flag
   * @param enabled True to enable firmware upgrade mode
   * @return ResourceResult indicating success or failure with error details
   * @details Enables or disables firmware upgrade mode.
   *          When enabled, the server enters a minimal state
   *          accepting only essential update-related requests.
   */
  ResourceResult setFirmwareUpgradeFlag(bool enabled);

  /**
   * @brief Handle incoming client requests
   * @details Processes incoming HTTP requests and routes them to appropriate
   *          handlers. Should be called regularly in the main loop.
   *          Handles request authentication, routing, and response generation.
   */
  void handleClient();

  /**
   * @brief Stop web server and cleanup resources
   * @details Terminates the web server and releases all allocated resources.
   *          Ensures proper cleanup of handlers and services.
   *          Should be called before system shutdown or reset.
   */
  void stop();

  /**
   * @brief Check if web manager is initialized
   * @return true if initialized, false otherwise
   * @details Indicates whether the web manager has been properly initialized
   *          and is ready to handle requests.
   */
  bool isInitialized() const { return _initialized; }

  /**
   * @brief Get reference to underlying web server
   * @return Reference to ESP8266WebServer instance
   * @note Direct server access should be used with caution
   * @details Provides access to the underlying web server instance.
   *          Should be used carefully to avoid bypassing WebManager's
   *          security and routing mechanisms.
   */
  ESP8266WebServer& getServer() { return *_server; }

  /**
   * @brief Check if a route exists for given path and method
   * @param path The URL path to check
   * @param method The HTTP method to check
   * @return true if route exists, false otherwise
   * @details Verifies if a handler is registered for the specified
   *          path and HTTP method combination.
   */
  bool hasExistingRoute(const String& path, HTTPMethod method) const {
    return hasRoute(path, method);
  }

  /**
   * @brief Enter OTA (Over The Air) update mode
   * @return ResourceResult indicating success or failure with error details
   * @details Switches the server into OTA update mode:
   *          - Disables non-essential services
   *          - Sets up minimal routing
   *          - Prepares for firmware/filesystem updates
   */
  ResourceResult beginUpdateMode();

  /**
   * @brief Cleanup resources and reset state
   * @details Releases allocated resources and resets internal state:
   *          - Stops the web server
   *          - Releases handler resources
   *          - Clears routes and cached data
   */
  void cleanup();

  /**
   * @brief Handle config value update requests
   * @details Processes POST requests to /admin/config/setConfigValue
   *          for updating individual configuration values.
   */
  void handleSetConfigValue();

  /**
   * @brief Get the update mode start time (for timeout recovery)
   */
  unsigned long getUpdateModeStartTime() const { return m_updateModeStartTime; }

  /**
   * @brief Get the update mode timeout (for timeout recovery)
   */
  unsigned long getUpdateModeTimeout() const { return m_updateModeTimeout; }

  /**
   * @brief Reset the update mode start time (for timeout recovery)
   */
  void resetUpdateModeStartTime() { m_updateModeStartTime = 0; }

private:
  static const size_t MAX_ACTIVE_HANDLERS = 4; ///< Maximum number of cached handlers

  /**
   * @struct HandlerCacheEntry
   * @brief Cache entry for request handlers
   * @details Stores handler instances and their metadata for efficient reuse
   */
  struct HandlerCacheEntry {
    std::unique_ptr<BaseHandler> handler; ///< Unique pointer to handler instance
    unsigned long lastAccess;             ///< Timestamp of last handler access
    String handlerType;                   ///< Type identifier for the handler
  };

  /**
   * @brief Private constructor for singleton pattern
   * @details Initializes member variables and sets up basic configuration.
   *          Private to enforce singleton pattern.
   */
  WebManager();

  /**
   * @brief Destructor
   * @details Ensures proper cleanup of resources when WebManager is destroyed.
   */
  ~WebManager();

  // Delete copy constructor and assignment operator
  WebManager(const WebManager&) = delete;
  WebManager& operator=(const WebManager&) = delete;

  // Core lifecycle methods (web_manager.cpp)
  /**
   * @brief Internal client handling implementation
   * @details Handles the actual client request processing with memory
   *          monitoring and handler initialization.
   */
  void handleClientInternal();

  // Initialization methods (web_manager_init.cpp)
  /**
   * @brief Initialize web services
   * @return ResourceResult indicating success or failure with error details
   * @details Sets up all required web services and handlers.
   *          Initializes core services needed for web server operation.
   */
  ResourceResult setupServices();

  /**
   * @brief Configure middleware
   * @details Sets up authentication and other middleware components.
   *          Configures request preprocessing and security measures.
   */
  void setupMiddleware();

  /**
   * @brief Set up minimal services for update mode
   * @return ResourceResult indicating success or failure with error details
   * @details Initializes only essential services needed for
   *          update mode operation.
   */
  ResourceResult setupMinimalServices();

  // Route management (web_manager_routes.cpp)
  /**
   * @brief Set up URL routes
   * @details Configures all URL routes and their corresponding handlers.
   *          Maps URLs to appropriate handler methods.
   */
  void setupRoutes();

  /**
   * @brief Set up minimal routes for update mode
   * @details Configures essential routes needed for updates.
   *          Removes non-essential routes to ensure system stability
   *          during update process.
   */
  void setupMinimalRoutes();

  /**
   * @brief Check if route exists
   * @param path URL path to check
   * @param method HTTP method to check
   * @return true if route exists, false otherwise
   * @details Internal method to verify route existence.
   *          Used by public hasExistingRoute method.
   */
  bool hasRoute(const String& path, HTTPMethod method) const;

  /**
   * @brief Remove existing route
   * @param path URL path to remove
   * @param method HTTP method to remove
   * @details Removes a route from the router configuration.
   *          Used during cleanup and route reconfiguration.
   */
  void removeRoute(const String& path, HTTPMethod method);

  // Request handlers (web_manager_handlers.cpp)
  /**
   * @brief Handle update flag setting requests
   * @details Processes POST requests to /admin/config/update
   *          in both normal and minimal modes.
   *          Validates and applies update settings.
   */
  void handleSetUpdate();

  /**
   * @brief Send error response with details
   * @param code HTTP status code
   * @param message Error message
   * @details Formats and sends an error response to the client
   *          with appropriate status code and message.
   */
  void sendErrorResponse(int code, const String& message);

  /**
   * @brief Validate update request
   * @param json Request body
   * @param fileSystemUpdate Output parameter for filesystem update flag
   * @param firmwareUpdate Output parameter for firmware update flag
   * @param updateMode Output parameter for update mode flag
   * @return ResourceResult indicating success or failure with error details
   * @details Validates update request parameters and sets appropriate flags
   *          for the update process.
   */
  ResourceResult validateUpdateRequest(const String& json, bool& fileSystemUpdate,
                                       bool& firmwareUpdate, bool& updateMode);

  /**
   * @brief Prepare system for update mode
   * @param fileSystemUpdate Whether to update filesystem
   * @param firmwareUpdate Whether to update firmware
   * @param updateMode Whether to enter update mode
   * @return true if preparation successful, false otherwise
   * @details Configures system for update process based on
   *          specified update types and mode.
   */
  bool prepareUpdateMode(bool fileSystemUpdate, bool firmwareUpdate, bool updateMode);

  /**
   * @brief Convert HTTP method to string representation
   * @param method HTTP method to convert
   * @return String representation of HTTP method
   * @details Converts HTTPMethod enum values to their string
   *          representation for logging and debugging.
   */
  static String methodToString(HTTPMethod method);

  // Handler cache management (web_manager_cache.cpp)
  /**
   * @brief Initialize remaining handlers
   * @details Sets up non-essential handlers that weren't initialized
   *          during the initial setup phase.
   */
  void initializeRemainingHandlers();

  /**
   * @brief Cleanup non-essential handlers
   * @details Releases resources for handlers that aren't needed
   *          in minimal operation mode.
   */
  void cleanupNonEssentialHandlers();

  /**
   * @brief Clean up all handlers
   * @details Releases resources for all handlers and resets
   *          handler-related state. Called during cleanup
   *          and before entering update mode.
   */
  void cleanupHandlers();

  /**
   * @brief Add handler to cache, removing oldest if necessary
   * @param handler Handler to add
   * @param handlerType Type identifier for the handler
   * @details Manages the handler cache using LRU policy:
   *          - Adds new handler to cache
   *          - Evicts oldest handler if cache is full
   *          - Updates access timestamps
   */
  void cacheHandler(std::unique_ptr<BaseHandler> handler, const String& handlerType);

  /**
   * @brief Get handler from cache if it exists
   * @param handlerType Type identifier for the handler
   * @return Pointer to handler or nullptr if not found
   * @details Searches the cache for a handler matching the specified type.
   *          Updates access timestamp if handler is found.
   */
  BaseHandler* getCachedHandler(const String& handlerType);

  /**
   * @brief Remove least recently used handler if cache is full
   * @details Implements the cache eviction policy:
   *          - Identifies least recently used handler
   *          - Removes it from cache
   *          - Frees associated resources
   */
  void evictOldestHandler();

  /**
   * @brief Update last access time for handler
   * @param handlerType Type identifier for the handler
   * @details Updates the access timestamp for a cached handler
   *          to maintain proper LRU ordering.
   */
  void updateHandlerAccess(const String& handlerType);

  // Static file serving (web_manager_static.cpp)
  /**
   * @brief Serve a static file from LittleFS
   * @param path Path to the file in LittleFS
   * @param contentType MIME type for the file
   * @param cacheControl Cache control header
   * @details Serves static files without using the built-in serveStatic
   *          to avoid MD5 calculation issues that can cause crashes.
   */
  void serveStaticFile(const String& path, const String& contentType, const String& cacheControl);

  // Utility methods (web_manager_utils.cpp)
  /**
   * @brief Check if device is in captive portal AP mode
   * @return true if in AP mode or disconnected
   * @details Determines if the device should show captive portal:
   *          - Checks WiFi mode
   *          - Validates connection status
   *          - Considers AP activation
   */
  bool isCaptivePortalAPActive() const;

  // Member variables
  std::list<HandlerCacheEntry> m_handlerCache; ///< LRU cache of handlers

  // Configuration constants
  unsigned long m_updateModeTimeout = 60000; ///< Update mode timeout (1 minute)
  unsigned long m_updateModeStartTime = 0;   ///< Update mode start timestamp
  static constexpr size_t BUFFER_SIZE = 256; ///< Response buffer size
  static char s_responseBuffer[BUFFER_SIZE]; ///< Static response buffer

  bool m_handlersInitialized{false};                         ///< Handler initialization flag
  std::unique_ptr<ESP8266WebServer> _server;                 ///< Web server instance
  std::unique_ptr<WebRouter> _router;                        ///< URL router
  std::unique_ptr<WebAuth> _auth;                            ///< Authentication service
  std::unique_ptr<CSSService> _cssService;                   ///< CSS management service
  std::unique_ptr<WebOTAHandler> _otaHandler;                ///< OTA update handler
  std::unique_ptr<AdminMinimalHandler> _minimalAdminHandler; ///< Minimal admin interface
  SensorManager* _sensorManager;                             ///< Sensor management

  // On-demand handlers
  std::unique_ptr<StartpageHandler> _startHandler;         ///< Start page handler
  std::unique_ptr<AdminHandler> _adminHandler;             ///< Admin interface handler
  std::unique_ptr<SensorHandler> _sensorHandler;           ///< Sensor data handler
  std::unique_ptr<AdminSensorHandler> _adminSensorHandler; ///< Sensor admin handler
  std::unique_ptr<LogHandler> _logHandler;                 ///< Logging handler
  std::unique_ptr<WiFiSetupHandler> _wifiSetupHandler;     ///< WiFi setup handler
#if USE_DISPLAY
  std::unique_ptr<AdminDisplayHandler> _displayHandler; ///< Display admin handler
#endif

  bool _initialized{false}; ///< Initialization state flag
  uint16_t _port;           ///< Server port number
};

// Global instance declaration
extern WebManager& webManager;

#endif // WEB_MANAGER_H
