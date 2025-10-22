#ifndef MANAGER_BASE_H
#define MANAGER_BASE_H

#include "../logger/logger.h"
#include "../managers/manager_types.h"
#include "../utils/result_types.h"

/**
 * @class Manager
 * @brief Base class for all managers providing common state management
 * @details Provides core functionality for manager initialization, state
 * tracking, error handling, and health monitoring.
 */
class Manager {
public:
  /**
   * @brief Construct a new Manager object
   * @param name Name identifier for the manager
   */
  explicit Manager(const char* name) : m_name(name) {
    m_status.setState(ManagerState::UNINITIALIZED);
  }

  /**
   * @brief Virtual destructor for proper cleanup
   */
  virtual ~Manager() = default;

  /**
   * @brief Initialize the manager
   * @return Result indicating success or failure
   */
  TypedResult<ResourceError, void> init() { return initializeWithStateTracking(); }

  /**
   * @brief Check if manager is initialized and healthy
   * @return true if manager is in a healthy state
   */
  inline bool isHealthy() const { return m_status.isHealthy(); }

  /**
   * @brief Get current manager state
   * @return Current ManagerState
   */
  inline ManagerState getState() const { return m_status.state; }

  /**
   * @brief Get last error if any
   * @return ManagerError containing error details
   */
  inline const ManagerError& getLastError() const { return m_status.lastError; }

  /**
   * @brief Get manager name
   * @return Manager name string
   */
  inline const String& getName() const { return m_name; }

  /**
   * @brief Initialize manager with state tracking
   * @return Result of initialization
   */
  TypedResult<ResourceError, void> initializeWithStateTracking() {
    setState(ManagerState::INITIALIZING);
    recordInitMemory();

    auto result = initialize();
    if (!result.isSuccess()) {
      setError("Initialisierung fehlgeschlagen: " + result.getMessage(), 1000);
      return result;
    }

    setState(ManagerState::INITIALIZED);
    return TypedResult<ResourceError, void>::success();
  }

protected:
  /**
   * @brief Initialize the manager
   * @return Result indicating success or failure
   */
  virtual TypedResult<ResourceError, void> initialize() = 0;

  /**
   * @brief Set manager state with logging
   * @param state New state to set
   */
  void setState(ManagerState state) {
    m_status.setState(state);
    logger.debug(F("BaseM"), m_name + ": Status gewechselt zu " + stateToString(state));
  }

  /**
   * @brief Set error state with message
   * @param message Error message
   * @param code Error code
   */
  void setError(const String& message, uint16_t code) {
    m_status.setError(message, code);
    logger.error(F("BaseM"), m_name + ": " + message + " (Code: " + String(code) + ")");
  }

  /**
   * @brief Track memory at initialization
   */
  inline void recordInitMemory() { m_status.freeHeapOnInit = ESP.getFreeHeap(); }

  /**
   * @brief Check if memory is critically low
   * @return true if memory is below safe threshold
   */
  inline bool isMemoryCritical() const { return ESP.getFreeHeap() < LOW_MEMORY_THRESHOLD; }

  /**
   * @brief Update manager state based on health check
   * @param isHealthy Current health status
   */
  void updateHealth(bool isHealthy) {
    if (!isHealthy && m_status.state == ManagerState::INITIALIZED) {
      setState(ManagerState::ERROR);
    } else if (isHealthy && m_status.state == ManagerState::ERROR) {
      setState(ManagerState::INITIALIZED);
    }
  }

private:
  static constexpr uint32_t LOW_MEMORY_THRESHOLD = 4096; ///< 4KB minimum memory threshold
  const String m_name;                                   ///< Manager name identifier
  ManagerStatus m_status;                                ///< Current manager status

  /**
   * @brief Convert manager state to string representation
   * @param state Manager state to convert
   * @return String representation of the state
   */
  static String stateToString(ManagerState state) {
    switch (state) {
    case ManagerState::UNINITIALIZED:
      return F("NICHT INITIALISIERT");
    case ManagerState::INITIALIZING:
      return F("INITIALISIERE");
    case ManagerState::INITIALIZED:
      return F("INITIALISIERT");
    case ManagerState::ERROR:
      return F("FEHLER");
    case ManagerState::MINIMAL:
      return F("MINIMAL");
    case ManagerState::SUSPENDED:
      return F("PAUSIERT");
    default:
      return F("UNBEKANNT");
    }
  }
};

#endif // MANAGER_BASE_H
