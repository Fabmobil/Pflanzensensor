/**
 * @file web_auth.h
 * @brief Authentication and authorization handling
 * @details Provides comprehensive authentication and authorization
 * functionality for the web interface, including:
 *          - Basic and token-based authentication
 *          - Role-based access control
 *          - Session management
 *          - Security logging
 */

#ifndef WEB_AUTH_H
#define WEB_AUTH_H

#include <ESP8266WebServer.h>

#include <map>

#include "managers/manager_config.h"
#include "utils/result_types.h"

/**
 * @enum AuthType
 * @brief Supported authentication types
 * @details Defines the available authentication mechanisms:
 *          - NONE: No authentication required
 *          - BASIC: HTTP Basic Authentication
 *          - TOKEN: Session token-based authentication
 */
enum class AuthType { NONE, BASIC, TOKEN };

/**
 * @enum UserRole
 * @brief User roles for authorization
 * @details Defines the available user roles for access control:
 *          - NONE: No permissions
 *          - USER: Basic user access
 *          - ADMIN: Administrative access
 */
enum class UserRole { NONE, USER, ADMIN };

/**
 * @struct SessionInfo
 * @brief Structure to store session information
 * @details Contains all necessary information for tracking user sessions:
 *          - Username for identification
 *          - Role for access control
 *          - Last access time for timeout management
 *          - Session token for authentication
 */
struct SessionInfo {
  String username;           ///< User's login name
  UserRole role;             ///< User's role level
  unsigned long lastAccess;  ///< Timestamp of last activity
  String token;              ///< Session authentication token

  /**
   * @brief Default constructor
   * @details Initializes a session with no permissions and empty credentials
   */
  SessionInfo()
      : username(""), role(UserRole::NONE), lastAccess(0), token("") {}
};

/**
 * @class WebAuth
 * @brief Authentication and authorization manager
 * @details Handles all aspects of user authentication and authorization:
 *          - Credential validation
 *          - Session management
 *          - Access control
 *          - Security logging
 */
class WebAuth {
 public:
  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @details Initializes the authentication manager with server reference
   *          and sets up internal data structures.
   */
  explicit WebAuth(ESP8266WebServer& server);

  /**
   * @brief Decode Base64 encoded string
   * @param input Base64 encoded string
   * @return Decoded string
   * @details Decodes Base64 encoded credentials used in Basic Authentication
   */
  String base64_decode(const String& input);

  /**
   * @brief Authenticate incoming request
   * @param requiredRole Minimum required role (default: USER)
   * @return true if authentication successful, false otherwise
   * @details Validates the incoming request's credentials:
   *          - Checks for authentication headers
   *          - Validates credentials or token
   *          - Verifies user has required role
   *          - Updates session information
   */
  bool authenticate(UserRole requiredRole = UserRole::USER);

  /**
   * @brief Set credentials for basic auth
   * @param username Username to set
   * @param password Password to set
   * @param role User role to assign
   * @details Stores or updates user credentials:
   *          - Securely stores password
   *          - Associates role with username
   *          - Updates existing user if present
   */
  void setCredentials(const String& username, const String& password,
                      UserRole role);

  /**
   * @brief Create new session for user
   * @param username Username for session
   * @param role User role for session
   * @return Session token string
   * @details Creates a new authenticated session:
   *          - Generates secure random token
   *          - Stores session information
   *          - Manages session limits
   *          - Updates access timestamps
   */
  String createSession(const String& username, UserRole role);

  /**
   * @brief Validate session token
   * @param token Session token to validate
   * @return true if session valid, false otherwise
   * @details Checks if session token is valid:
   *          - Verifies token exists
   *          - Checks session timeout
   *          - Updates last access time
   */
  bool validateSession(const String& token);

  /**
   * @brief Cleanup expired sessions
   * @details Removes invalid sessions:
   *          - Deletes timed out sessions
   *          - Frees associated resources
   *          - Maintains session limits
   */
  void cleanupSessions();

  /**
   * @brief Check if request has admin authentication
   * @return true if authenticated as admin, false otherwise
   * @details Convenience method for checking admin access:
   *          - Requires ADMIN role
   *          - Uses standard authentication process
   */
  bool checkAuthentication() {
    return authenticate(UserRole::ADMIN);  // Default to requiring admin role
  }

 private:
  ESP8266WebServer& _server;              ///< Reference to web server instance
  std::map<String, String> _credentials;  ///< Username to password mapping
  std::map<String, UserRole> _roles;      ///< Username to role mapping
  std::map<String, SessionInfo> _sessions;  ///< Token to session info mapping

  static const unsigned long SESSION_TIMEOUT =
      3600000;                           ///< Session timeout (1 hour)
  static const size_t MAX_SESSIONS = 5;  ///< Maximum concurrent sessions

  /**
   * @brief Check basic auth credentials
   * @param username Username from request
   * @param password Password from request
   * @return true if credentials valid, false otherwise
   * @details Validates Basic Authentication credentials:
   *          - Checks username exists
   *          - Verifies password matches
   *          - Confirms account is active
   */
  bool checkBasicAuth(const String& username, const String& password);

  /**
   * @brief Check token auth
   * @param token Token from request
   * @return true if token valid, false otherwise
   * @details Validates token-based authentication:
   *          - Verifies token exists
   *          - Checks session validity
   *          - Updates session timestamp
   */
  bool checkTokenAuth(const String& token);

  /**
   * @brief Generate random token
   * @param length Token length (default: 32)
   * @return Random token string
   * @details Generates secure random token:
   *          - Uses cryptographically secure RNG
   *          - Ensures token uniqueness
   *          - Applies proper encoding
   */
  String generateToken(size_t length = 32);

  /**
   * @brief Get auth type from request
   * @return Detected auth type
   * @details Determines authentication type from request:
   *          - Checks Authorization header
   *          - Identifies auth scheme
   *          - Handles missing auth info
   */
  AuthType getAuthType();

  /**
   * @brief Send auth required response
   * @details Sends appropriate authentication challenge:
   *          - Sets WWW-Authenticate header
   *          - Returns 401 status
   *          - Includes auth scheme info
   */
  void requestAuth();

  /**
   * @brief Log authentication attempt
   * @param username Username attempting authentication
   * @param success Whether authentication was successful
   * @details Records authentication activity:
   *          - Logs timestamp and username
   *          - Records success/failure
   *          - Tracks failed attempt counts
   */
  void logAuthAttempt(const String& username, bool success);
};

#endif  // WEB_AUTH_H
