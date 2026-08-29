/**
 * @file css_service.h
 * @brief CSS management service for web interface
 * @details Provides functionality for managing CSS files and routes in the web
 * interface, including custom CSS editing, backup, and serving of CSS content.
 */
#ifndef CSS_SERVICE_H
#define CSS_SERVICE_H

#include <vector>

#include "../handler/base_handler.h"

/**
 * @class CSSService
 * @brief Service for managing CSS files and routes
 * @details Handles all CSS-related functionality including:
 *          - Serving CSS files
 *          - CSS file editing through web interface
 *          - CSS backup and restoration
 *          - Custom CSS management
 */
class CSSService : public BaseHandler {
public:
  /**
   * @brief Constructor
   * @param server Reference to web server instance
   * @details Initializes the CSS service and sets up required modules
   */
  explicit CSSService(ESPWebServer& server);

  /**
   * @brief Register CSS service routes with router
   * @param router Reference to router instance
   * @return Router result indicating success or failure
   * @details Sets up routes for:
   *          - CSS file serving
   *          - Static asset management
   * @note Override onRegisterRoutes for custom logic.
   */
  RouterResult onRegisterRoutes(WebRouter& router) override;

  /**
   * @brief Handle GET requests for CSS resources
   * @param uri Request URI identifying the CSS resource
   * @param query Map of query parameters
   * @return HandlerResult indicating success or failure
   * @details Handles requests for:
   *          - CSS file content
   *          - CSS editor page
   *          - Default CSS content
   */
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;

  /**
   * @brief Handle POST requests for CSS operations
   * @param uri Request URI identifying the operation
   * @param params Map of POST parameters
   * @return HandlerResult indicating success or failure
   * @details Handles:
   *          - CSS content updates
   *          - CSS backup creation
   *          - CSS restoration
   */
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

private:
  /**
   * @struct CSSModule
   * @brief Represents a CSS module with its properties
   * @details Contains metadata and path information for a CSS module.
   *          Used to manage different CSS components in the system.
   */
  struct CSSModule {
    String id;   ///< Unique identifier for the module
    String name; ///< Human-readable name of the module
    String path; ///< File system path to the CSS file

    /**
     * @brief Constructor for CSSModule
     * @param i Unique identifier for the module
     * @param n Human-readable name of the module
     * @param p File system path to the CSS file
     */
    CSSModule(const String& i, const String& n, const String& p) : id(i), name(n), path(p) {}
  };

  std::vector<CSSModule> _modules; ///< Collection of CSS modules

  /**
   * @brief Initialize CSS modules
   * @details Sets up the default and custom CSS modules.
   *          Called during service initialization.
   */
  void initModules();
};

#endif // CSS_SERVICE_H
