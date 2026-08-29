/**
 * @file sensor_measurement_cycle.h
 * @brief Manages the measurement cycle of individual sensors
 * @details Implements a state machine that controls the complete lifecycle of a
 * sensor measurement, including initialization, measurement, data processing,
 * and cleanup.
 */
#ifndef SENSOR_MEASUREMENT_CYCLE_MANAGER_H
#define SENSOR_MEASUREMENT_CYCLE_MANAGER_H

#include <memory>

#include "managers/manager_config.h"
#include "sensor_measurement_state.h"
#include "sensors/sensor_manager_limiter.h"
#include "sensors/sensors.h"

/**
 * @class SensorMeasurementCycleManager
 * @brief Manages the complete measurement cycle for a single sensor
 * @details Implements a state machine that handles all aspects of sensor
 * measurement, including timing, initialization, measurement, data processing,
 * and error handling.
 */
class SensorMeasurementCycleManager {
public:
  /**
   * @brief Constructor for the measurement cycle manager
   * @param sensor Pointer to the sensor to manage
   */
  explicit SensorMeasurementCycleManager(Sensor* sensor);

  /**
   * @brief Updates the measurement cycle state machine
   * @return true if the cycle is complete, false if still in progress
   * @details This is the main method that should be called regularly to
   * progress through the measurement cycle states.
   */
  bool updateMeasurementCycle();

  /**
   * @brief Einen Durchlauf der Zustandsmaschine ausführen
   * @details Kapselt, was früher im SensorManager stand: prüfen ob etwas zu
   *          tun ist, den Zyklus weiterschalten und Zustandswechsel für die
   *          Debug-Ausgabe verfolgen. Die dafür nötigen Zustände liegen jetzt
   *          hier statt in einer std::map<String, SensorStateLog>.
   */
  void tick();

  /**
   * @brief Resets the measurement cycle to its initial state
   */
  void reset();

  /**
   * @brief Gets the current state of the measurement cycle
   * @return Current MeasurementState
   */
  MeasurementState getCurrentState() const;

  /**
   * @brief Gets the last error message if any
   * @return Reference to the last error message string
   */
  const String& getLastError() const;

  /**
   * @brief Checks if it's time for the next measurement
   * @return true if a new measurement is due
   */
  bool isDue() const { return m_state.isDue(); }

  /**
   * @brief Startet sofort eine Messung, ohne Wartezeit
   * @details Bricht einen eventuell laufenden eigenen Zyklus sauber ab, gibt
   *          den Slot frei und setzt den Sensor auf "sofort fällig". Das
   *          gesetzte Kennzeichen isForced() sorgt dafür, dass die
   *          Zustandsmaschine anschließend in jedem Schleifendurchlauf
   *          weitergeschaltet wird statt nur einmal pro Sekunde, und dass die
   *          Wartezeit nach der Initialisierung entfällt.
   *
   *          Vorher wurde hier nur der Zustand zurückgesetzt. Steckte der
   *          Sensor noch mitten im Zyklus, behielt er dabei seinen Messslot —
   *          und blockierte sich dann selbst, bis der Slot nach 45 s
   *          zwangsweise freigegeben wurde.
   */
  void forceImmediateMeasurement();

  /**
   * @brief Bricht einen laufenden Messzyklus ab und gibt den Slot frei
   * @details Deinitialisiert den Sensor, falls er für diesen Zyklus
   *          initialisiert wurde, gibt einen gehaltenen Messslot frei und
   *          setzt die Zustandsmaschine auf WAITING_FOR_DUE zurück. Der
   *          nächste reguläre Messzeitpunkt bleibt unverändert.
   */
  void abortCycle();

  /**
   * @brief Läuft gerade eine manuell ausgelöste Messung?
   */
  bool isForced() const { return m_forced; }

private:
  // Timeouts and delays
  static constexpr unsigned long INIT_TIMEOUT = 5000; ///< Timeout for initialization (5 seconds)
  static constexpr unsigned long MEASURE_TIMEOUT = 30000; ///< Timeout for measurement (30 seconds)
  static constexpr unsigned long ERROR_RETRY_DELAY =
      1000;                                        ///< Delay before retrying after error (1 second)
  static constexpr unsigned long INIT_DELAY = 100; ///< Delay after initialization (100ms)
  static constexpr unsigned long WARMUP_DELAY = 100;    ///< Delay after warmup (100ms)
  static constexpr unsigned long DEBUG_INTERVAL = 5000; ///< Interval between debug logs (5 seconds)
  static constexpr unsigned long SLOT_RETRY_DELAY =
      50; ///< Delay between slot attempts (50ms, reduced from 100ms)
  static constexpr unsigned long SLOT_TIMEOUT = 50000; ///< Maximum slot hold time (50 seconds)

  /// Obergrenze für den Abstand zwischen zwei Versuchen eines dauerhaft
  /// fehlerhaften Sensors (30 Minuten).
  static constexpr unsigned long MAX_RETRY_BACKOFF = 1800000UL;
  /// Obergrenze für die Verdopplungsstufe, damit der Zähler nicht wegläuft.
  static constexpr uint8_t MAX_RETRY_LEVEL = 8;

  // Member variables
  Sensor* m_sensor;                    ///< Pointer to the managed sensor
  MeasurementStateInfo m_state;        ///< Current state information
  MeasurementState m_lastState;        ///< Previous state for transition tracking
  std::vector<float> m_currentResults; ///< Current measurement results
  // **CRITICAL FIX: Remove local MeasurementData copy to prevent memory
  // corruption** We'll work directly with the sensor's MeasurementData instead
  MeasurementState m_lastLoggedState{MeasurementState::WAITING_FOR_DUE}; ///< für Debug-Ausgabe
  bool m_lastUpdateResult{false};                                        ///< für Debug-Ausgabe
  unsigned long m_lastDebugTime{0};        ///< Last debug message timestamp
  unsigned long m_cycleStartTime{0};       ///< Start time of current measurement cycle
  unsigned long m_lastSlotAttemptTime{0};  ///< Last attempt to acquire measurement slot
  unsigned long m_slotRequestStartTime{0}; ///< When current slot request started
  bool m_forced{false}; ///< Manuell ausgelöste Messung: ohne Wartezeit durchziehen
  /// Verdopplungsstufe des Wiederholungsabstands, 0 = normaler Messtakt.
  /// Wächst mit jeder erschöpften Versuchsreihe, wird bei jeder erfolgreichen
  /// Messung wieder auf 0 gesetzt.
  uint8_t m_retryLevel{0};

  // State handlers (defined in separate files)

  /**
   * @brief Handles the WAITING_FOR_DUE state
   * @return true if state processing is complete
   */
  bool handleWaitingForDue();

  /**
   * @brief Handles the WAITING_FOR_SLOT state
   */
  void handleWaitingForSlot();

  /**
   * @brief Handles the WAITING_FOR_DELAY state
   */
  void handleWaitingForDelay();

  /**
   * @brief Handles the WARMUP state
   */
  void handleWarmup();

  /**
   * @brief Handles the MEASURING state
   */
  void handleMeasuring();

  // Initialization handlers (defined in
  // sensor_measurement_cycle_initialization.cpp)

  /**
   * @brief Handles the INITIALIZING state
   */
  void handleInitializing();

  // Data processing handlers (defined in
  // sensor_measurement_cycle_data_processing.cpp)

  /**
   * @brief Handles the PROCESSING state
   */
  void handleProcessing();

  /**
   * @brief Handles the DEINITIALIZING state
   */
  void handleDeinitializing();

  /**
   * @brief Logs the measurement results
   */
  void logMeasurementResults();

  // Error handling (defined in sensor_measurement_cycle_error_handling.cpp)

  /**
   * @brief Handles the ERROR state
   */
  void handleError();

  /**
   * @brief Handles unknown states
   */
  void handleUnknownState();

  /**
   * @brief Handles errors that occur during state processing
   * @param error Description of the error
   */
  void handleStateError(const String& error);

  /**
   * @brief Deactivates the sensor after fatal errors
   */
  /**
   * @brief Plant den nächsten Versuch mit wachsendem Abstand
   * @details Ersetzt die frühere dauerhafte Abschaltung des Sensors. Der
   *          Sensor bleibt aktiviert; nur der Abstand zwischen zwei Versuchen
   *          verdoppelt sich mit jeder erschöpften Versuchsreihe, gedeckelt
   *          auf MAX_RETRY_BACKOFF. Eine erfolgreiche Messung setzt die Stufe
   *          zurück.
   *
   *          Vorher rief handleError() hier setEnabled(false) auf.
   *          SensorManager::updateMeasurements() überspringt deaktivierte
   *          Sensoren, und zurück auf true kam der Sensor nur über das
   *          Webinterface oder einen Neustart - ein Wackelkontakt legte damit
   *          einen Kanal still, sichtbar allein an einer Logzeile.
   */
  void scheduleRetryWithBackoff();

  /**
   * @brief Checks if the slot request has timed out
   * @return true if the slot request has exceeded the timeout
   */
  bool checkSlotTimeout();

  /**
   * @brief Initiates a new slot request
   */
  void startSlotRequest();
};

#endif // SENSOR_MEASUREMENT_CYCLE_MANAGER_H
