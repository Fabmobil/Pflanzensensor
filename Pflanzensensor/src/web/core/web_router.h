/**
 * @file web_router.h
 * @brief URL routing and request handling for web server
 * @details Provides a routing system for handling HTTP requests:
 *          - URL pattern matching
 *          - HTTP method routing
 *          - Middleware support
 *          - Static file serving
 *          - Memory-aware operation
 */

#ifndef WEB_ROUTER_H
#define WEB_ROUTER_H

#include <ESP8266WebServer.h>

#include <functional>
#include <memory>
#include <vector>

#include "logger/logger.h"
#include "sensors/sensor_count.h"
#include "utils/result_types.h"

/// Type alias for router operation results
using RouterResult = TypedResult<RouterError, void>;
/// Type alias for route handler functions
using HandlerCallback = std::function<void()>;
/// Type alias for middleware functions
using MiddlewareCallback = std::function<bool(HTTPMethod, String)>;

/**
 * @struct Route
 * @brief Represents a URL route with its handler
 * @details Contains all information needed to match and handle a route:
 *          - URL pattern
 *          - HTTP method
 *          - Handler function
 */
struct Route {
  String url;              ///< URL pattern to match
  HTTPMethod method;       ///< HTTP method to match
  HandlerCallback handler; ///< Function to handle the route

  /**
   * @brief Constructor for Route
   * @param u URL pattern
   * @param m HTTP method
   * @param h Handler function
   * @details Uses move semantics for efficient handler storage
   */
  Route(const String& u, HTTPMethod m, HandlerCallback h)
      : url(u), method(m), handler(std::move(h)) {} // Use move semantics for handler
};

/**
 * @class WebRouter
 * @brief Manages URL routing and request handling
 * @details Provides comprehensive routing functionality:
 *          - Route registration and matching
 *          - Middleware processing
 *          - Memory management
 *          - Request handling
 *          - Static file serving
 */
class WebRouter {
public:
  // Configuration constants
  /// Maximum total routes based on enabled sensors
  static constexpr size_t MAX_ROUTES = 50;
  /// Maximum number of middleware functions
  static constexpr size_t MAX_MIDDLEWARE = 8;
  /// Minimum required heap space for operation
  static constexpr uint32_t MIN_FREE_HEAP = 4096;

  /**
   * @brief Constructor
   * @param server Reference to ESP8266WebServer instance
   * @details Initializes router with server reference and
   *          prepares internal data structures
   */
  explicit WebRouter(ESP8266WebServer& server);

  /**
   * @brief Convert HTTP method to string representation
   * @param method The HTTP method to convert
   * @return String representation of the HTTP method
   * @details Converts HTTPMethod enum to human-readable string
   *          for logging and debugging purposes
   */
  String methodToString(HTTPMethod method);

  /**
   * @brief Add route for specific HTTP method
   * @param method HTTP method to handle
   * @param url URL pattern to match
   * @param handler Function to handle the route
   * @return RouterResult indicating success or failure
   * @details Registers a new route with error checking:
   *          - Validates memory availability
   *          - Checks route limits
   *          - Ensures unique routes
   */
  RouterResult addRoute(HTTPMethod method, const String& url, HandlerCallback handler);

  /**
   * @brief Add middleware function
   * @param middleware Function to process before route handlers
   * @details Registers middleware for request preprocessing:
   *          - Authentication
   *          - Logging
   *          - Request modification
   */
  void addMiddleware(MiddlewareCallback middleware);

  /**
   * @brief Configure static file serving
   * @param urlPrefix URL prefix for static files
   * @param fs Filesystem to serve from
   * @param path Base path in filesystem
   * @param cache Whether to enable caching
   * @details Sets up static file serving with options:
   *          - URL path mapping
   *          - Cache control
   *          - File system integration
   */
  void serveStatic(const String& urlPrefix, fs::FS& fs, const String& path, bool cache = true);

  /**
   * @brief Handle incoming HTTP request
   * @param method HTTP method of request
   * @param url Requested URL
   * @return true if request was handled, false otherwise
   * @details Processes request through routing system:
   *          - Executes middleware
   *          - Matches routes
   *          - Calls handlers
   *          - Handles errors
   */
  bool handleRequest(HTTPMethod method, const String& url);

  /**
   * @brief Check if route exists
   * @param path URL path to check
   * @param method HTTP method to check
   * @return true if route exists, false otherwise
   * @details Checks for existing route while considering memory:
   *          - Validates memory availability
   *          - Searches route collection
   *          - Matches exact path and method
   */
  bool hasRoute(const String& path, HTTPMethod method) const {
    // Early return if memory is low
    if (ESP.getFreeHeap() < MIN_FREE_HEAP) {
      return false;
    }

    for (const auto& route : _routes) {
      if (route.url == path && route.method == method) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Check router health status
   * @return true if router is healthy, false otherwise
   * @details Verifies router operational status:
   *          - Checks memory availability
   *          - Monitors heap fragmentation
   *          - Ensures system stability
   */
  bool isHealthy() const {
    return ESP.getFreeHeap() >= MIN_FREE_HEAP && ESP.getHeapFragmentation() < 70;
  }

private:
  ESP8266WebServer& _server;                   ///< Reference to web server
  std::vector<Route> _routes;                  ///< Collection of registered routes
  std::vector<MiddlewareCallback> _middleware; ///< Registered middleware functions

  /**
   * @brief Check if route limit is exceeded
   * @return true if at route limit, false otherwise
   * @details Ensures router stays within configured limits
   */
  bool exceedsRouteLimit() const { return _routes.size() >= MAX_ROUTES; }

  /**
   * @brief Check if middleware limit is exceeded
   * @return true if at middleware limit, false otherwise
   * @details Ensures middleware stack stays within limits
   */
  bool exceedsMiddlewareLimit() const { return _middleware.size() >= MAX_MIDDLEWARE; }

  /**
   * @brief Execute middleware chain
   * @param method HTTP method being processed
   * @param url URL being processed
   * @return true if middleware chain succeeds, false otherwise
   * @details Processes request through middleware:
   *          - Executes each middleware in order
   *          - Handles middleware results
   *          - Stops on first failure
   */
  bool executeMiddleware(HTTPMethod method, const String& url);

  /**
   * @brief Find matching route
   * @param method HTTP method to match
   * @param url URL to match
   * @return Pointer to matching Route or nullptr
   * @details Searches for matching route:
   *          - Matches exact URL and method
   *          - Returns first match found
   *          - Handles memory constraints
   */
  Route* findRoute(HTTPMethod method, const String& url);

  /**
   * @brief Log route registration
   * @param method HTTP method being registered
   * @param url URL being registered
   * @details Records route registration:
   *          - Logs route details
   *          - Tracks registration time
   *          - Monitors system state
   */
  void logRouteRegistration(HTTPMethod method, const String& url);

  /**
   * @brief Check memory availability
   * @return true if enough memory available, false otherwise
   * @details Verifies system has enough memory:
   *          - Checks free heap space
   *          - Ensures stable operation
   */
  bool hasEnoughMemory() const { return ESP.getFreeHeap() >= MIN_FREE_HEAP; }
};

#endif // WEB_ROUTER_H
