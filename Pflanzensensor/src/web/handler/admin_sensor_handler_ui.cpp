/**
 * @file admin_sensor_handler_ui.cpp
 * @brief Implementation of UI rendering and page generation
 */

#include <map>

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_resource.h"
#include "sensors/sensor_analog.h"
#include "utils/helper.h"
#include "web/core/components.h"

void AdminSensorHandler::handleSensorConfig() {
  logger.debug(F("AdminSensorHandler"), F("handleSensorConfig() aufgerufen"));

  if (!validateRequest()) {
    logger.debug(F("AdminSensorHandler"),
                 F("Authentifizierung in handleSensorConfig fehlgeschlagen"));
    this->sendError(401, F("Authentifizierung erforderlich"));
    return;
  }
  std::vector<String> css = {"admin"};
  std::vector<String> js = {"admin", "admin_sensors"};
  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/sensors",
      [this]() {
        // Flower Status Sensor Selection Card
        renderFlowerStatusSensorCard();
#if USE_LED_TRAFFIC_LIGHT
        // LED Traffic Light Settings Card
        generateAndSendLedTrafficLightSettingsCard();
#endif
        sendChunk(F("<div class='admin-grid'>"));
        if (_sensorManager.isHealthy()) {
          const auto& sensors = _sensorManager.getSensors();
          for (const auto& sensor : sensors) {
            if (!sensor)
              continue;
            if (!sensor->isInitialized() || !sensor->isEnabled())
              continue;
            String id = sensor->getId();
            SensorConfig& config = sensor->mutableConfig();
            if (config.activeMeasurements > SensorConfig::MAX_MEASUREMENTS) {
              logger.warning(F("AdminSensorHandler"),
                             F("Clamping activeMeasurements for sensor ") + id + F(" from ") +
                                 String(config.activeMeasurements) + F(" to ") +
                                 String(SensorConfig::MAX_MEASUREMENTS));
              config.activeMeasurements = SensorConfig::MAX_MEASUREMENTS;
            }
            size_t nRows = config.activeMeasurements < config.measurements.size()
                               ? config.activeMeasurements
                               : config.measurements.size();

            // Begin sensor card
            sendChunk(F("<div class='card sensor-card' data-sensor='"));
            sendChunk(id);
            sendChunk(F("'>"));

            // Sensor card title as <h2>
            sendChunk(F("<div class='card-header'>"));
            sendChunk(F("<h2 class='sensor-id-title'>"));
            sendChunk(id);
            sendChunk(F("-Sensor</h2>"));
            sendChunk(F("</div>"));

            // Measurement interval input (sensor-wide)
            sendChunk(F("<div class='card-section status-row'>"));
            sendChunk(F("Messintervall: <input type='number' step='any' "
                        "name='interval_"));
            sendChunk(id);
            sendChunk(F("' value='"));
            sendChunk(String(int(config.measurementInterval / 1000)));
            sendChunk(F("' class='measurement-interval-input' data-sensor-id='"));
            sendChunk(id);
            sendChunk(F("'> Sekunden"));

            // Messen button for the whole sensor
            sendChunk(
                F(" <button type='button' class='button-primary measure-button' data-sensor='"));
            sendChunk(id);
            sendChunk(F("'>"));
            sendChunk(F("Messen</button>"));
            sendChunk(F("</div>"));

            // Render all measurements for this sensor
            for (size_t i = 0; i < nRows; ++i) {
              if (i > 0)
                sendChunk(F("<hr>")); // separation between measurement cards
              renderSensorMeasurementRow(sensor.get(), i, nRows);
            }

            sendChunk(F("</div>")); // end sensor card
          }
        }
        sendChunk(F("</div>")); // end admin-grid
      },
      css, js);
}

void AdminSensorHandler::generateThresholdConfig(Sensor* sensor, size_t measurementIdx) {
  if (!sensor)
    return;
  String id = sensor->getId();
  const auto& config = sensor->config();
  if (measurementIdx >= config.activeMeasurements || measurementIdx >= config.measurements.size())
    return;
  // Only output the container div; JS will generate the inputs and
  // visualization
  sendChunk(F("<div id='threshold_"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(measurementIdx));
  sendChunk(F("' class='threshold-container'></div>"));
}

// New function: renderSensorMeasurementRow
void AdminSensorHandler::renderSensorMeasurementRow(Sensor* sensor, size_t i, size_t nRows) {
  String id = sensor->getId();
  auto measurementData = sensor->getMeasurementData();
  SensorConfig& config = sensor->mutableConfig();
  bool analog = isAnalogSensor(sensor);

  // Begin measurement card
  sendChunk(F("<div class='measurement-card'>"));
  // Name label and input
  sendChunk(F("<div class='name-row'><label for='name_"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("'>Sensorname:</label> "));
  sendChunk(F("<input type='text' size='20' class='measurement-name' id='name_"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("' name='name_"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("' value='"));
  sendChunk(config.measurements[i].name);
  sendChunk(F("' placeholder='Messwert Name'></div>"));

// Inverted scale checkbox
#if USE_ANALOG
  if (analog) {
    sendChunk(F("<div class='card-section inverted-section'>"));
    sendChunk(F("<label><input type='checkbox' name='inverted_"));
    sendChunk(id);
    sendChunk(F("_"));
    sendChunk(String(i));
    sendChunk(F("' class='analog-inverted-checkbox' data-sensor-id='"));
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    sendChunk(F("'"));
    if (config.measurements[i].inverted) {
      sendChunk(F(" checked"));
    }
    sendChunk(F("> Skala invertieren (hohe Rohwerte = niedrige Prozente)</label>"));
    sendChunk(F("</div>"));
  }
#endif

  // Absolute min/max values section
  sendChunk(F("<div class='card-section minmax-section'>"));
  // Last value, error count, and measurement button
  sendChunk(F("<div class='card-section status-row'>"));
  sendChunk(F("Letzter Messwert: <input readonly class='readonly-value' "
              "data-sensor='"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("' value='"));
  if (measurementData.isValid() && i < measurementData.activeValues &&
      i < measurementData.values.size() && i < SensorConfig::MAX_MEASUREMENTS) {
    sendChunk(String(int(measurementData.values[i])));
  } else {
    sendChunk(F("--"));
  }
  sendChunk(F("'> "));
  sendChunk(measurementData.units[i]);
  sendChunk(F(" (Fehler: "));
  sendChunk(String(sensor->getErrorCount()));
  sendChunk(F(") "));
  sendChunk(F("</div>"));
  sendChunk(F("Min: <input readonly class='readonly-value absolute-min-input' "
              "data-sensor-id='"));
  sendChunk(id);
  sendChunk(F("' data-measurement-index='"));
  sendChunk(String(i));
  sendChunk(F("' value='"));
  if (config.measurements[i].absoluteMin != INFINITY) {
    sendChunk(String(config.measurements[i].absoluteMin, 2));
  } else {
    sendChunk(F("--"));
  }
  sendChunk(F("'> "));
  sendChunk(measurementData.units[i]);
  sendChunk(F(" | Max: <input readonly class='readonly-value "
              "absolute-max-input' data-sensor-id='"));
  sendChunk(id);
  sendChunk(F("' data-measurement-index='"));
  sendChunk(String(i));
  sendChunk(F("' value='"));
  if (config.measurements[i].absoluteMax != -INFINITY) {
    sendChunk(String(config.measurements[i].absoluteMax, 2));
  } else {
    sendChunk(F("--"));
  }
  sendChunk(F("'> "));
  sendChunk(measurementData.units[i]);
  sendChunk(F(" <button type='button' class='button-secondary reset-minmax-button warning' "
              "data-sensor-id='"));
  sendChunk(id);
  sendChunk(F("' data-measurement-index='"));
  sendChunk(String(i));
  sendChunk(F("' style='margin-left:8px;'>Zurücksetzen</button>"));

  // Thresholds (per measurement)
  sendChunk(F("<div class='status-row'><h3>Schwellwerte</h3></div>"));
  sendChunk(F("<div class='card-section threshold-row'>"));
  sendChunk(F("<div class='threshold-inputs'>"));
  sendChunk(F("<label>Gelb min: <input type='number' step='any' name='"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("_yellowLow' value='"));
  sendChunk(String(int(config.measurements[i].limits.yellowLow)));
  sendChunk(F("' class='threshold-input'></label>"));
  sendChunk(F("<label>Grün min: <input type='number' step='any' name='"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("_greenLow' value='"));
  sendChunk(String(int(config.measurements[i].limits.greenLow)));
  sendChunk(F("' class='threshold-input'></label>"));
  sendChunk(F("<label>Grün max: <input type='number' step='any' name='"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("_greenHigh' value='"));
  sendChunk(String(int(config.measurements[i].limits.greenHigh)));
  sendChunk(F("' class='threshold-input'></label>"));
  sendChunk(F("<label>Gelb max: <input type='number' step='any' name='"));
  sendChunk(id);
  sendChunk(F("_"));
  sendChunk(String(i));
  sendChunk(F("_yellowHigh' value='"));
  sendChunk(String(int(config.measurements[i].limits.yellowHigh)));
  sendChunk(F("' class='threshold-input'></label>"));
  sendChunk(F("</div>"));
  float lastValue = (measurementData.isValid() && i < measurementData.activeValues &&
                     i < measurementData.values.size() && i < SensorConfig::MAX_MEASUREMENTS)
                        ? measurementData.values[i]
                        : NAN;
  sendChunk(F("<div class='threshold-slider-container' "));
  if (!isnan(lastValue)) {
    sendChunk(F("data-last-value='"));
    sendChunk(String(lastValue, 2));
    sendChunk(F("' "));
  }
  sendChunk(F(">"));
  generateThresholdConfig(sensor, i);
  sendChunk(F("</div>"));
  sendChunk(F("</div>")); // end threshold-row

  sendChunk(F("</div>"));

  // Analog min/max and raw value rows
#if USE_ANALOG
  if (analog) {
    AnalogSensor* analogSensor = static_cast<AnalogSensor*>(sensor);
    sendChunk(F("<div class='card-section minmax-section'>"));
    sendChunk(F("<div class='status-row'><h3>Rohwerte Berechnungslimits:</h3></div>"));
    sendChunk(F("Min: <input type='number' step='any' name='min_"));
    sendChunk(id);
    sendChunk(F("_"));
    sendChunk(String(i));
    sendChunk(F("' value='"));
    sendChunk(String(int(analogSensor->getMinValue(i))));
    // Add readonly-value class when calibrationMode is active so the field
    // is visually the same as other readonly fields on initial render
    if (config.measurements[i].calibrationMode) {
      sendChunk(F("' class='analog-min-input readonly-value' data-sensor-id='"));
    } else {
      sendChunk(F("' class='analog-min-input' data-sensor-id='"));
    }
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    // Disable manual editing when autocalibration is enabled for this measurement
    if (config.measurements[i].calibrationMode) {
      sendChunk(F("' disabled> | Letzter: <input readonly class='readonly-value' value='"));
    } else {
      sendChunk(F("'> | Letzter: <input readonly class='readonly-value' value='"));
    }
    int rawValue = analogSensor->getLastRawValue(i);
    if (rawValue >= 0) {
      sendChunk(String(rawValue));
    } else {
      sendChunk(F("--"));
    }
    sendChunk(F("'> | Max: <input type='number' step='any' name='max_"));
    sendChunk(id);
    sendChunk(F("_"));
    sendChunk(String(i));
    sendChunk(F("' value='"));
    sendChunk(String(int(analogSensor->getMaxValue(i))));
    // Add readonly-value class when calibrationMode is active so the field
    // is visually the same as other readonly fields on initial render
    if (config.measurements[i].calibrationMode) {
      sendChunk(F("' class='analog-max-input readonly-value' data-sensor-id='"));
    } else {
      sendChunk(F("' class='analog-max-input' data-sensor-id='"));
    }
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    if (config.measurements[i].calibrationMode) {
      sendChunk(F("' disabled>"));
    } else {
      sendChunk(F("'>"));
    }

    // Autokalibrierung checkbox and reset button (no separate persisted autocal fields)
    sendChunk(F("<div class='card-section autocal-section'>"));
    sendChunk(F("<label><input type='checkbox' name='autocal_"));
    sendChunk(id);
    sendChunk(F("_"));
    sendChunk(String(i));
    sendChunk(F("' class='analog-autocal-checkbox' data-sensor-id='"));
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    sendChunk(F("'"));
    if (config.measurements[i].calibrationMode) {
      sendChunk(F(" checked"));
    }
    sendChunk(F("> Autokalibrierung aktivieren<a "
                "href=\"https://github.com/Fabmobil/Pflanzensensor/wiki/"
                "automatische-Kalibrierung\" target=\"_blank\">❔</a></label>"));
    sendChunk(F("</div>"));

    // Autocal duration select (6h,12h,1d,3d,1w,1M) — only show when autocal active
    if (config.measurements[i].calibrationMode) {
      sendChunk(F("<div class='card-section autocal-duration-section'>"));
      sendChunk(F("<label>Halbwertszeit: "));
      sendChunk(F("<select class='analog-autocal-duration' data-sensor-id='"));
      sendChunk(id);
      sendChunk(F("' data-measurement-index='"));
      sendChunk(String(i));
      sendChunk(F("'>"));
      uint32_t cur = config.measurements[i].autocalHalfLifeSeconds;
      auto opt = [&](uint32_t v, const char* label) {
        sendChunk(F("<option value='"));
        sendChunk(String(v));
        sendChunk(F("'"));
        if (cur == v)
          sendChunk(F(" selected"));
        sendChunk(F(">"));
        sendChunk(String(label));
        sendChunk(F("</option>"));
      };
      opt(21600, "6 Stunden");
      opt(43200, "12 Stunden");
      opt(86400, "1 Tag");
      opt(259200, "3 Tage");
      opt(604800, "1 Woche");
      opt(2592000, "1 Monat");
      sendChunk(F("</select></label></div>"));
    }

    sendChunk(F("</div>"));

    // Raw min/max values section
    sendChunk(F("<div class='card-section minmax-section'>"));
    sendChunk(F("<div class='status-row'><h3>Rohwerte Extremmesswerte:</h3></div>"));
    sendChunk(F("Min: <input readonly class='readonly-value absolute-raw-min-input' "
                "data-sensor-id='"));
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    sendChunk(F("' value='"));
    if (config.measurements[i].absoluteRawMin != INT_MAX) {
      sendChunk(String(config.measurements[i].absoluteRawMin));
    } else {
      sendChunk(F("--"));
    }
    sendChunk(F("'> | Max: <input readonly class='readonly-value "
                "absolute-raw-max-input' data-sensor-id='"));
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    sendChunk(F("' value='"));
    if (config.measurements[i].absoluteRawMax != INT_MIN) {
      sendChunk(String(config.measurements[i].absoluteRawMax));
    } else {
      sendChunk(F("--"));
    }
    sendChunk(F("'> <button type='button' class='button-secondary reset-raw-minmax-button warning' "
                "data-sensor-id='"));
    sendChunk(id);
    sendChunk(F("' data-measurement-index='"));
    sendChunk(String(i));
    sendChunk(F("' style='margin-left:8px;'>Zurücksetzen</button>"));
    sendChunk(F("</div>"));
  }
#endif

  sendChunk(F("</div>")); // end measurement-card
  yield();
}

void AdminSensorHandler::renderFlowerStatusSensorCard() {
  logger.debug(F("AdminSensorHandler"), F("renderFlowerStatusSensorCard()"));

  sendChunk(F("<div class='card'>"));
  sendChunk(F("<h2>Gesicht der Blume</h2>"));
  sendChunk(F("<p>Wähle den Sensor, der das Gesicht der Blume auf der Startseite steuert:</p>"));

  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='flower-status-sensor'>Sensor:</label>"));
  sendChunk(F("<select id='flower-status-sensor' class='form-control'>"));

  // Get currently configured sensor
  String currentSensor = ConfigMgr.getFlowerStatusSensor();

  // Iterate through all sensors and their measurements
  if (_sensorManager.isHealthy()) {
    const auto& sensors = _sensorManager.getSensors();
    for (const auto& sensor : sensors) {
      if (!sensor)
        continue;
      if (!sensor->isInitialized() || !sensor->isEnabled())
        continue;

      String sensorId = sensor->getId();
      SensorConfig& config = sensor->mutableConfig();
      size_t nRows = config.activeMeasurements < config.measurements.size()
                         ? config.activeMeasurements
                         : config.measurements.size();

      for (size_t i = 0; i < nRows; ++i) {
        String optionValue = sensorId + F("_") + String(i);
        String displayName = sensorId + F(" - ") + config.measurements[i].name;

        sendChunk(F("<option value='"));
        sendChunk(optionValue);
        sendChunk(F("'"));
        if (optionValue == currentSensor) {
          sendChunk(F(" selected"));
        }
        sendChunk(F(">"));
        sendChunk(displayName);
        sendChunk(F("</option>"));
      }
    }
  }

  sendChunk(F("</select>"));
  sendChunk(F("</div>"));

  sendChunk(F("</div>"));
  yield();
}

void AdminSensorHandler::generateAndSendLedTrafficLightSettingsCard() {
#if USE_LED_TRAFFIC_LIGHT
  sendChunk(F("<div class='card'><h3>LED-Ampel Einstellungen</h3>"));
  sendChunk(F("<form method='post' action='/admin/updateSettings' "
              "class='config-form'>"));
  sendChunk(F("<input type='hidden' name='section' value='led_traffic_light'>"));

  // Mode selection
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label>LED-Ampel Modus:</label>"));
  sendChunk(F("<select name='led_traffic_light_mode'>"));
  sendChunk(F("<option value='0'"));
  if (ConfigMgr.getLedTrafficLightMode() == 0)
    sendChunk(F(" selected"));
  sendChunk(F(">Modus 0: LED-Ampel aus</option>"));
  sendChunk(F("<option value='1'"));
  if (ConfigMgr.getLedTrafficLightMode() == 1)
    sendChunk(F(" selected"));
  sendChunk(F(">Modus 1: Alle Messungen anzeigen</option>"));
  sendChunk(F("<option value='2'"));
  if (ConfigMgr.getLedTrafficLightMode() == 2)
    sendChunk(F(" selected"));
  sendChunk(F(">Modus 2: Nur ausgewählte Messung anzeigen</option>"));
  sendChunk(F("</select>"));
  sendChunk(F("</div>"));

  // Measurement selection (only visible in mode 2)
  sendChunk(F("<div class='form-group' id='measurement_selection_group'"));
  if (ConfigMgr.getLedTrafficLightMode() != 2) {
    sendChunk(F(" style='display: none;'"));
  }
  sendChunk(F(">"));
  sendChunk(F("<label for='led_traffic_light_measurement'>Ausgewählte "
              "Messung:</label>"));
  sendChunk(F("<select name='led_traffic_light_measurement' "
              "id='led_traffic_light_measurement'>"));
  sendChunk(F("<option value=''>-- Messung auswählen --</option>"));

  // Get available measurements from sensor manager
  extern std::unique_ptr<SensorManager> sensorManager;
  if (sensorManager) {
    for (const auto& sensor : sensorManager->getSensors()) {
      if (sensor && sensor->isEnabled()) {
        String sensorId = sensor->getId();
        String sensorName = sensor->getName();

        // Get all measurements for this sensor
        for (size_t i = 0; i < sensor->config().activeMeasurements; i++) {
          String measurementName = sensor->getMeasurementName(i);
          String fieldName = sensor->config().measurements[i].fieldName;

          // Create measurement identifier
          String measurementId = sensorId + "_" + String(i);

          // Create display name
          String displayName = sensorName;
          if (!measurementName.isEmpty()) {
            displayName += " - " + measurementName;
          }
          if (!fieldName.isEmpty()) {
            displayName += " (" + fieldName + ")";
          }

          sendChunk(F("<option value='"));
          sendChunk(measurementId);
          sendChunk(F("'"));
          if (ConfigMgr.getLedTrafficLightSelectedMeasurement() == measurementId)
            sendChunk(F(" selected"));
          sendChunk(F(">"));
          sendChunk(displayName);
          sendChunk(F("</option>"));
        }
      }
    }
  }
  sendChunk(F("</select>"));
  sendChunk(F("</div>"));

  // Save handled automatically via AJAX; keep form for fallback but remove visible submit button
  sendChunk(F("</form>"));

  // Add JavaScript to show/hide measurement selection based on mode
  sendChunk(F("<script>"));
  sendChunk(F("document.addEventListener('DOMContentLoaded', function() {"));
  sendChunk(F("  const modeSelect = "
              "document.querySelector('select[name=\"led_traffic_light_mode\"]');"));
  sendChunk(F("  const measurementGroup = "
              "document.getElementById('measurement_selection_group');"));
  sendChunk(F("  function toggleMeasurementSelection() {"));
  sendChunk(F("    if (modeSelect.value === '2') {")); // Changed to '2' for mode 2
  sendChunk(F("      measurementGroup.style.display = 'block';"));
  sendChunk(F("    } else {"));
  sendChunk(F("      measurementGroup.style.display = 'none';"));
  sendChunk(F("    }"));
  sendChunk(F("  }"));
  sendChunk(F("  modeSelect.addEventListener('change', toggleMeasurementSelection);"));
  sendChunk(F("  toggleMeasurementSelection();"));
  sendChunk(F("});"));
  sendChunk(F("</script>"));

  sendChunk(F("</div>"));
#endif // USE_LED_TRAFFIC_LIGHT
}
