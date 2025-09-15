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
  explicit CSSService(ESP8266WebServer& server);

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
  HandlerResult handleGet(const String& uri,
                          const std::map<String, String>& query) override;

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
  HandlerResult handlePost(const String& uri,
                           const std::map<String, String>& params) override;

  /**
   * @brief Get custom CSS content
   * @return String containing the custom CSS content
   * @details Retrieves the current custom CSS content from storage.
   *          Returns empty string if no custom CSS exists.
   */
  String getCustomCSS();

  /**
   * @brief Update custom CSS content
   * @param css New CSS content to store
   * @return true if update was successful, false on error
   * @details Validates and stores the new CSS content.
   *          Creates a backup of existing CSS before update.
   */
  bool updateCustomCSS(const String& css);

  /**
   * @brief Get default CSS content
   * @return String containing the default CSS content
   * @details Returns the built-in default CSS content.
   *          This content cannot be modified.
   */
  String getDefaultCSS();

 private:
  /**
   * @struct CSSModule
   * @brief Represents a CSS module with its properties
   * @details Contains metadata and path information for a CSS module.
   *          Used to manage different CSS components in the system.
   */
  struct CSSModule {
    String id;    ///< Unique identifier for the module
    String name;  ///< Human-readable name of the module
    String path;  ///< File system path to the CSS file

    /**
     * @brief Constructor for CSSModule
     * @param i Unique identifier for the module
     * @param n Human-readable name of the module
     * @param p File system path to the CSS file
     */
    CSSModule(const String& i, const String& n, const String& p)
        : id(i), name(n), path(p) {}
  };

  std::vector<CSSModule> _modules;  ///< Collection of CSS modules

  /**
   * @brief Initialize CSS modules
   * @details Sets up the default and custom CSS modules.
   *          Called during service initialization.
   */
  void initModules();

  /**
   * @brief Handle CSS editor page requests
   * @details Serves the CSS editor interface with current CSS content
   *          and editing controls.
   */
  void handleEditor();

  /**
   * @brief Handle CSS save requests
   * @details Processes CSS content updates, performs validation,
   *          and manages backups.
   */
  void handleSave();

  /**
   * @brief Create backup of CSS file
   * @param path Path to CSS file to backup
   * @return true if backup was successful, false on error
   * @details Creates a timestamped backup copy of the specified CSS file.
   *          Maintains a limited number of backups.
   */
  bool createBackup(const String& path) const;

  /**
   * @brief Load CSS content from file
   * @param path Path to CSS file to load
   * @return CSS content or empty string on error
   * @details Reads and returns the contents of a CSS file.
   *          Handles file system errors gracefully.
   */
  String loadCSS(const String& path) const;

  /**
   * @brief Save CSS content to file
   * @param path Path where CSS content should be saved
   * @param content CSS content to save
   * @return true if save was successful, false on error
   * @details Writes CSS content to the specified file.
   *          Creates parent directories if needed.
   */
  bool saveCSS(const String& path, const String& content) const;

  /**
   * @brief Get module by ID
   * @param id Module identifier to search for
   * @return Pointer to module if found, nullptr otherwise
   * @details Searches the modules collection for a module matching
   *          the specified identifier.
   */
  const CSSModule* getModule(const String& id) const;
};

#endif  // CSS_SERVICE_H
