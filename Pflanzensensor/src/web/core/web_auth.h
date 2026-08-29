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

#include "utils/platform_compat.h"

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
  explicit WebAuth(ESPWebServer& server);

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
   * @brief Check if request has admin authentication
   * @return true if authenticated as admin, false otherwise
   * @details Convenience method for checking admin access:
   *          - Requires ADMIN role
   *          - Uses standard authentication process
   */
  bool checkAuthentication() {
    return authenticate(UserRole::ADMIN); // Default to requiring admin role
  }

  /**
   * @brief Notfallpasswort - gilt zusätzlich zum eingestellten Adminpasswort
   *
   * Zweck: Zugang zum Gerät, wenn das gesetzte Adminpasswort vergessen wurde.
   * Ohne dieses Passwort bliebe nur ein Factory-Reset über die serielle
   * Schnittstelle oder ein erneutes Flashen.
   *
   * ACHTUNG - Tragweite: Der Wert steht im Klartext im öffentlichen Quelltext
   * und lässt sich aus jeder veröffentlichten firmware.bin per "strings"
   * auslesen. Er ist damit kein Geheimnis. Jedes Gerät mit dieser Firmware ist
   * für jeden im selben Netz administrierbar, unabhängig davon, welches
   * Adminpasswort eingestellt wurde. Wer das nicht möchte, muss den Wert vor
   * dem Bauen ändern (dann ist er allerdings auch nicht mehr allgemein
   * bekannt und der Wiederherstellungszweck entfällt für Dritte).
   */
  static constexpr const char* EMERGENCY_ADMIN_PASSWORD = "FabmobilNotfallpasswort!11";

  /// Mindestabstand zwischen zwei Warnungen zur Notfallpasswort-Nutzung
  static constexpr unsigned long EMERGENCY_WARN_INTERVAL_MS = 60000;

  /**
   * @brief Zentrale Prüfung der Admin-Zugangsdaten
   * @param server Webserver, dessen Authorization-Header geprüft wird
   * @return true wenn eingestelltes Adminpasswort ODER Notfallpasswort passt
   * @details Einziger Ort, an dem entschieden wird, ob eine Anfrage
   *          Administratorrechte hat. Alle Aufrufstellen im Projekt gehen
   *          hierdurch - vorher stand
   *          server.authenticate("admin", ConfigMgr.getAdminPassword().c_str())
   *          an acht Stellen einzeln im Code, was bei einer Änderung der
   *          Regeln zwangsläufig auseinandergelaufen wäre.
   *
   *          Die Benutzung des Notfallpassworts wird protokolliert, damit sie
   *          im Log sichtbar ist.
   */
  static bool checkAdminCredentials(ESPWebServer& server);

private:
  // Die drei std::map (_credentials, _roles, _sessions) samt Session-Logik sind
  // entfernt: sie wurden nie befüllt. authenticate() prüft direkt gegen
  // ConfigMgr.getAdminPassword().
  ESPWebServer& _server; ///< Reference to web server instance
};

#endif // WEB_AUTH_H
