/**
 * @file log_handler.h
 * @brief Handler for log viewing and WebSocket streaming
 * @details Provides comprehensive logging functionality including:
 *          - Real-time log streaming via WebSocket
 *          - Log viewing interface
 *          - Log level management
 *          - Log cleanup
 *          - Client management
 */

#pragma once

#include <list>
#include <vector>

#include "configs/config.h"
#if USE_WEBSOCKET
#include "../services/websocket.h"
#endif
#include "base_handler.h"

class WebManager; ///< Forward declaration for web manager
class WebAuth;    ///< Forward declaration for authentication service
class CSSService; ///< Forward declaration for CSS service

/**
 * @class LogHandler
 * @brief Handles log viewing and WebSocket streaming functionality
 * @details Manages all aspects of system logging:
 *          - Log collection and storage
 *          - Real-time streaming
 *          - Client connections
 *          - Log cleanup
 *          - Interface generation
 */
class LogHandler : public BaseHandler {
  friend class WebManager; // Allow WebManager to access private members

public:
  // Constants
#if USE_WEBSOCKET
  static constexpr uint16_t WS_PORT = 81; ///< WebSocket server port
#endif
  static constexpr unsigned long LOG_CLEANUP_INTERVAL = 60000; ///< Cleanup interval (60 seconds)

  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS service
   * @details Initializes log handler with required services:
   *          - Server connection
   *          - Authentication
   *          - CSS management
   *          - Internal state
   */
  LogHandler(ESP8266WebServer& server, WebAuth& auth, CSSService& cssService)
      : BaseHandler(server),
        _auth(auth),
        _cssService(cssService),
        _lastCleanup(0),
        _cleaned(false),
        _initialized(false) {}

  /**
   * @brief Virtual destructor
   * @details Ensures proper cleanup:
   *          - Releases resources
   *          - Updates singleton state
   *          - Closes connections
   */
  virtual ~LogHandler() {
    cleanup();
    if (s_instance == this) {
      s_instance = nullptr;
      s_initialized = false;
    }
  }

  /**
   * @brief Get singleton instance of LogHandler
   * @param server Reference to web server instance
   * @param auth Reference to authentication service
   * @param cssService Reference to CSS service
   * @return Pointer to LogHandler instance
   * @details Implements singleton pattern:
   *          - Creates instance if needed
   *          - Returns existing instance
   *          - Ensures single point of access
   */
  static LogHandler* getInstance(ESP8266WebServer& server, WebAuth& auth, CSSService& cssService) {
    if (!s_instance) {
      s_instance = new LogHandler(server, auth, cssService);
      if (s_instance) {
        s_instance->_initialized = true;
        s_initialized = true;
      }
    }
    return s_instance;
  }

  /**
   * @brief Check if LogHandler is properly initialized
   * @return true if initialized, false otherwise
   */
  bool isInitialized() const { return _initialized && s_initialized; }

  /**
   * @brief Register log routes with the router
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Sets up routes for:
   *          - Log viewing page
   *          - WebSocket endpoint
   *          - Log management
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

  /**
   * @brief Handle GET requests
   * @param uri Request URI
   * @param query Query parameters
   * @return Handler result indicating success or failure
   * @details Processes GET requests for:
   *          - Log viewing
   *          - Log download
   *          - Status queries
   */
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;

  /**
   * @brief Handle POST requests
   * @param uri Request URI
   * @param params POST parameters
   * @return Handler result indicating success or failure
   * @details Processes POST requests for:
   *          - Log level changes
   *          - Log clearing
   *          - Configuration updates
   */
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

#if USE_WEBSOCKET
  /**
   * @brief Process WebSocket events and clean up logs
   * @details Performs periodic tasks:
   *          - Processes WebSocket messages
   *          - Cleans up old logs
   *          - Manages connections
   *          - Updates state
   */
  void loop();

  /**
   * @brief Clean up all WebSocket clients
   * @details Performs client cleanup:
   *          - Closes connections
   *          - Frees resources
   *          - Updates client list
   *          - Clears queues
   */
  void cleanupAllClients();

  /**
   * @brief Broadcast log message to all connected clients
   * @param level Log level of message
   * @param message Log message content
   * @details Sends log message to clients:
   *          - Formats message
   *          - Checks client status
   *          - Handles errors
   *          - Updates queues
   */
  void broadcastLog(LogLevel level, const String& message);

  /**
   * @brief Handle WebSocket events
   * @param num Client number
   * @param type Event type
   * @param payload Event data
   * @param length Payload length
   * @details Processes WebSocket events:
   *          - Connection management
   *          - Message handling
   *          - Error processing
   *          - State updates
   */
  void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
#endif

  /**
   * @brief Handle log viewing page request
   * @details Generates log viewing interface:
   *          - Log display
   *          - Level filters
   *          - Control panel
   *          - Real-time updates
   */
  void handleLogs();

  /**
   * @brief Clean up handler resources
   * @return true if cleanup was successful, false if already cleaned
   * @details Performs comprehensive cleanup:
   *          - Closes connections
   *          - Clears queues
   *          - Frees memory
   *          - Updates state
   */
  bool cleanup() override {
    if (!_cleaned) {
#if USE_WEBSOCKET
      cleanupAllClients();
      _messageQueue.clear();
#endif
      _content.clear();
      _cleaned = true;
      _initialized = false;
      return true;
    }
    return false;
  }

#if USE_WEBSOCKET
  /**
   * @struct QueuedMessage
   * @brief Structure for queued WebSocket messages
   * @details Contains message information:
   *          - Client identifier
   *          - Message content
   *          - Queue management
   */
  struct QueuedMessage {
    uint8_t clientId; ///< Identifier of target client
    String message;   ///< Message content to send

    /**
     * @brief Constructor for QueuedMessage
     * @param id Client identifier
     * @param msg Message content
     */
    QueuedMessage(uint8_t id, const String& msg) : clientId(id), message(msg) {}
  };
#endif

private:
  WebAuth& _auth;          ///< Reference to authentication service
  CSSService& _cssService; ///< Reference to CSS service
#if USE_WEBSOCKET
  std::list<uint8_t> _clients;              ///< List of connected client IDs
  std::vector<QueuedMessage> _messageQueue; ///< Queue of pending messages
#endif
  unsigned long _lastCleanup;    ///< Timestamp of last cleanup
  String _content;               ///< Current log content
  bool _cleaned;                 ///< Cleanup status flag
  bool _initialized;             ///< Instance initialization flag
  static LogHandler* s_instance; ///< Singleton instance pointer
  static bool s_initialized;     ///< Global initialization flag

#if USE_WEBSOCKET
  /**
   * @brief Initialize WebSocket server
   * @return true if initialization successful
   * @details Sets up WebSocket server:
   *          - Configures port
   *          - Sets handlers
   *          - Initializes state
   */
  bool initWebSocket();

  /**
   * @brief Clean up resources for a specific client
   * @param clientNum Client number to clean up
   * @details Performs client-specific cleanup:
   *          - Closes connection
   *          - Removes from lists
   *          - Clears messages
   */
  void cleanupClientResources(uint8_t clientNum);

  /**
   * @brief Handle client message
   * @param clientNum Client number
   * @param type Message type
   * @param data Message data
   * @details Processes client messages:
   *          - Validates format
   *          - Handles commands
   *          - Updates state
   *          - Sends responses
   */
  void handleClientMessage(uint8_t clientNum, const String& type, const String& data);

  /**
   * @brief Send log buffer to client
   * @param num Client number
   * @details Sends buffered logs:
   *          - Formats content
   *          - Handles chunking
   *          - Manages errors
   */
  void sendLogBuffer(uint8_t num);
#endif

  /**
   * @brief Clean up old logs
   * @details Manages log retention:
   *          - Removes old entries
   *          - Updates storage
   *          - Maintains limits
   */
  void cleanupLogs();

  /**
   * @brief Get color for log level
   * @param level Log level to get color for
   * @return Color string for the level
   * @details Maps log levels to colors:
   *          - Consistent color scheme
   *          - Visual differentiation
   *          - Accessibility consideration
   */
  String getLogLevelColor(LogLevel level) const;

protected:
  /**
   * @brief Custom cleanup logic for LogHandler
   * @details Ensures all resources are released and state is reset.
   */
  void onCleanup() override {
#if USE_WEBSOCKET
    cleanupAllClients();
    _messageQueue.clear();
#endif
    _content.clear();
    _initialized = false;
  }
};
