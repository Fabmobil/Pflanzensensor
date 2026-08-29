/**
 * @file web_helper_sensors.h
 * @brief Zentrale Hilfsklasse für Sensor-Zugriffe in Web-Handlern
 * @details Konsolidiert alle Sensor-Zugriffsmuster an einer Stelle:
 *          - Iteration über Sensoren
 *          - Suche nach Sensor-ID
 *          - Validierung von Messungs-Indizes
 *          - Filtern nach aktivierten Sensoren
 * 
 * Vorteile:
 * - DRY: Keine doppelten Schleifen mehr über Sensoren
 * - Konsistent: Einheitliche Filter-Logik
 * - Testbar: Isolierte Logik ohne Arduino-Abhängigkeiten
 */

#ifndef WEB_HELPER_SENSORS_H
#define WEB_HELPER_SENSORS_H

#include <Arduino.h>
#include <memory>
#include <vector>

// Forward declarations (aus manager_types.h und sensor_types.h)
class Sensor;
class SensorManager;

/**
 * @class SensorHelper
 * @brief Zentrale Hilfsklasse für Sensor-Zugriffe
 * @details Statische Methoden für alle wiederkehrenden Sensor-Operationen
 *          in Web-Handlern. Eliminiert Code-Duplikation und macht die
 *          Sensor-Zugriffslogik testbar.
 */
class SensorHelper {
public:
  /**
   * @brief Alle Sensoren abrufen (Kurzform)
   */
  static const std::vector<std::unique_ptr<Sensor>>& getAllSensors(SensorManager& manager);

  /**
   * @brief Nur aktivierte Sensoren abrufen
   * @return Vektor mit Pointern auf aktivierte Sensoren
   * @details Filtert deaktivierte Sensoren heraus, die im Web nicht
   *          angezeigt werden sollten.
   */
  static std::vector<Sensor*> getEnabledSensors(SensorManager& manager);

  /**
   * @brief Sensor nach ID finden
   * @param manager SensorManager-Instanz
   * @param sensorId Gesuchte Sensor-ID
   * @return Pointer auf Sensor oder nullptr wenn nicht gefunden
   */
  static Sensor* findById(SensorManager& manager, const String& sensorId);

  /**
   * @brief Sensor nach ID finden (Kurzform)
   */
  static Sensor* findById(const std::vector<std::unique_ptr<Sensor>>& sensors,
                          const String& sensorId);

  /**
   * @brief Prüfen ob Messungs-Index gültig ist
   * @param sensor Sensor-Instanz
   * @param measurementIndex Zu prüfender Index
   * @return true wenn Index im gültigen Bereich liegt
   */
  static bool isValidMeasurementIndex(const Sensor* sensor, size_t measurementIndex);

  /**
   * @brief Prüfen ob Sensor initialisiert und aktiviert ist
   * @return true wenn Sensor für Web-Zugriffe bereit ist
   */
  static bool isReady(const Sensor* sensor);

  /**
   * @brief Alle Sensor-IDs als kommaseparierten String
   * @details Für Debug-Zwecke oder Dropdown-Listen
   */
  static String getSensorIdsAsString(SensorManager& manager);

  /**
   * @brief Anzahl aktivierte Sensoren
   */
  static size_t getEnabledCount(SensorManager& manager);

  /**
   * @brief Prüfen ob irgendein Sensor aktiviert ist
   */
  static bool hasEnabledSensors(SensorManager& manager);

private:
  SensorHelper() = default; // Nur statische Methoden
};

#endif // WEB_HELPER_SENSORS_H