#ifndef MANAGER_TYPES_H
#define MANAGER_TYPES_H

#include <Arduino.h>

/**
 * @brief Forward declarations of manager classes
 */
class SensorManager;
class DisplayManager;
class WebManager;
class ResourceManager;
class ConfigManager;

/**
 * @brief Enum representing the state of a manager
 */
enum class ManagerState {
  UNINITIALIZED,  ///< Not yet initialized
  INITIALIZING,   ///< Currently initializing
  INITIALIZED,    ///< Successfully initialized and running
  ERROR,          ///< Encountered error, not operational
  MINIMAL,        ///< Running in minimal/failsafe mode
  SUSPENDED       ///< Temporarily suspended (e.g., low memory)
};

/**
 * @brief Structure to hold error information
 */
struct ManagerError {
  String message;           ///< Error message description
  unsigned long timestamp;  ///< Time when error occurred
  uint16_t code;            ///< Error code identifier

  /**
   * @brief Default constructor
   */
  inline ManagerError() : message(""), timestamp(0), code(0) {}

  /**
   * @brief Constructor with error message and code
   * @param msg Error message
   * @param errCode Error code
   */
  ManagerError(const String& msg, uint16_t errCode)
      : message(msg), timestamp(millis()), code(errCode) {}
};

/**
 * @brief Structure to track the state of each manager
 */
struct ManagerStatus {
  ManagerState state{ManagerState::UNINITIALIZED};  ///< Current manager state
  ManagerError lastError;            ///< Last error that occurred
  unsigned long stateChangeTime{0};  ///< Time of last state change
  uint32_t restartCount{0};          ///< Number of restarts
  uint32_t errorCount{0};            ///< Number of errors
  bool isMinimalMode{false};         ///< Running in minimal mode
  uint32_t freeHeapOnInit{0};        ///< Free heap at initialization

  /**
   * @brief Set new manager state
   * @param newState State to transition to
   */
  void setState(ManagerState newState) {
    state = newState;
    stateChangeTime = millis();
  }

  /**
   * @brief Set error state with message and code
   * @param message Error message
   * @param code Error code
   */
  void setError(const String& message, uint16_t code) {
    state = ManagerState::ERROR;
    lastError = ManagerError(message, code);
    stateChangeTime = millis();
    errorCount++;
  }

  /**
   * @brief Check if manager is in a healthy state
   * @return true if manager is healthy
   */
  bool isHealthy() const {
    return state == ManagerState::INITIALIZED ||
           (isMinimalMode && state == ManagerState::MINIMAL);
  }
};

/**
 * @brief Structure containing status of all managers
 */
struct SystemManagerState {
  ManagerStatus sensorManager;    ///< Sensor manager status
  ManagerStatus displayManager;   ///< Display manager status
  ManagerStatus webManager;       ///< Web manager status
  ManagerStatus resourceManager;  ///< Resource manager status
  ManagerStatus configManager;    ///< Config manager status

  unsigned long lastStateUpdate{0};  ///< Last state update timestamp
  bool inUpdateMode{false};          ///< System is in update mode
  bool inLowMemoryMode{false};       ///< System is in low memory mode
};
/**
 * @brief Global system state instance
 */
extern SystemManagerState g_managerState;

/**
 * @brief Helper class to track manager state changes
 */
class ManagerStateGuard {
 public:
  /**
   * @brief Constructor that sets temporary state
   * @param status Reference to manager status
   * @param newState Temporary state to set
   */
  ManagerStateGuard(ManagerStatus& status, ManagerState newState)
      : m_status(status), m_previousState(status.state) {
    m_status.setState(newState);
  }

  /**
   * @brief Destructor that restores previous state
   */
  ~ManagerStateGuard() { m_status.setState(m_previousState); }

 private:
  ManagerStatus& m_status;       ///< Reference to manager status
  ManagerState m_previousState;  ///< Previous state to restore
};

#endif  // MANAGER_TYPES_H
