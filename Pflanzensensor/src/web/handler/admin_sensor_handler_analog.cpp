/**
 * @file admin_sensor_handler_analog.cpp
 * @brief Analog sensor specific operations (min/max, inversion)
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"
#include "managers/manager_sensor_persistence.h"
#include "sensors/sensor_analog.h"
#include "utils/helper.h"

void AdminSensorHandler::handleAnalogInverted() {
#if USE_ANALOG
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("inverted")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  bool inverted = _server.arg("inverted") == "true";

  logger.debug(F("AdminSensorHandler"), F("handleAnalogInverted: sensor=") + sensorId +
                                            F(", measurement=") + String(measurementIndex) +
                                            F(", inverted=") + String(inverted));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(500,
                     F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  if (!isAnalogSensor(sensor)) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    logger.error(F("AdminSensorHandler"), F("Ungültiger Messindex: ") + String(measurementIndex) +
                                              F(", max erlaubt: ") +
                                              String(config.measurements.size() - 1));
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // If nothing changed, return OK (use config's inverted flag)
  if (config.measurements[measurementIndex].inverted == inverted) {
    logger.debug(F("AdminSensorHandler"),
                 F("Keine Änderungen für invertierten Zustand festgestellt"));
    sendJsonResponse(200, F("{\"success\":true}"));
    return;
  }

  // Update in-memory config
  config.measurements[measurementIndex].inverted = inverted;
  
  // Persist using unified setConfigValue approach
  String ns = PreferencesNamespaces::getSensorNamespace(sensorId);
  String key = PreferencesNamespaces::getSensorMeasurementKey(measurementIndex, "inv");
  auto result = ConfigMgr.setConfigValue(ns, key, inverted ? "true" : "false", ConfigValueType::BOOL);
  
  if (!result.isSuccess()) {
    logger.error(F("AdminSensorHandler"),
                 F("Fehler beim Aktualisieren des invertierten Zustands: ") + result.getMessage());
    sendJsonResponse(
        500,
        F("{\"success\":false,\"error\":\"Fehler beim Speichern des invertierten Zustands\"}"));
    return;
  }

  logger.info(F("AdminSensorHandler"), F("Analog invertiert für ") + sensorId + F("[") +
                                           String(measurementIndex) + F("]: invertiert=") +
                                           String(inverted));

  sendJsonResponse(200, F("{\"success\":true}"));
#else
  sendJsonResponse(400, F("{\"success\":false,\"error\":\"Analog sensors not enabled\"}"));
#endif
}

void AdminSensorHandler::handleAnalogMinMax() {
#if USE_ANALOG
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("min") || !_server.hasArg("max")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  float newMin = _server.arg("min").toFloat();
  float newMax = _server.arg("max").toFloat();

  logger.debug(F("AdminSensorHandler"), F("handleAnalogMinMax: sensor=") + sensorId +
                                            F(", measurement=") + String(measurementIndex) +
                                            F(", min=") + String(newMin) + F(", max=") +
                                            String(newMax));

  // Debug: print all incoming arguments
  for (int i = 0; i < _server.args(); ++i) {
    logger.debug(F("AdminSensorHandler"),
                 F("POST arg: ") + _server.argName(i) + F(" = ") + _server.arg(i));
  }

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(500,
                     F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  if (!isAnalogSensor(sensor)) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
    return;
  }

  AnalogSensor* analog = static_cast<AnalogSensor*>(sensor);
  // Use the config to check measurement count instead of protected method
  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    logger.error(F("AdminSensorHandler"), F("Ungültiger Messindex: ") + String(measurementIndex) +
                                              F(", max erlaubt: ") +
                                              String(config.measurements.size() - 1));
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // Disallow manual min/max updates while autocalibration is active.
  if (config.measurements[measurementIndex].calibrationMode) {
    logger.warning(F("AdminSensorHandler"),
                   F("Manuelle Min/Max-Änderung verweigert: Autokalibrierung aktiv"));
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Autokalibrierung aktiv - Min/Max nicht "
                            "manuell änderbar\"}"));
    return;
  }

  bool changed = false;
  if (newMin != analog->getMinValue(measurementIndex)) {
    analog->setMinValue(measurementIndex, newMin);
    changed = true;
  }
  if (newMax != analog->getMaxValue(measurementIndex)) {
    analog->setMaxValue(measurementIndex, newMax);
    changed = true;
  }
  if (changed) {
    logger.debug(F("AdminSensorHandler"),
                 F("Analog-Min/Max geändert, Konfiguration wird aktualisiert und persistiert"));
    // Persist min/max in config
    config.measurements[measurementIndex].minValue = analog->getMinValue(measurementIndex);
    config.measurements[measurementIndex].maxValue = analog->getMaxValue(measurementIndex);
    // Use atomic update for analog min/max values; persist integer-rounded
    int persistMin = static_cast<int>(roundf(config.measurements[measurementIndex].minValue));
    int persistMax = static_cast<int>(roundf(config.measurements[measurementIndex].maxValue));
    auto result = SensorPersistence::updateAnalogMinMaxInteger(
        sensorId, measurementIndex, persistMin, persistMax,
        config.measurements[measurementIndex].inverted);
    if (!result.isSuccess()) {
      logger.error(F("AdminSensorHandler"),
                   F("Fehler beim Aktualisieren von Analog-Min/Max: ") + result.getMessage());
      sendJsonResponse(
          500, F("{\"success\":false,\"error\":\"Fehler beim Speichern der Min/Max-Werte\"}"));
      return;
    }
    logger.debug(F("AdminSensorHandler"),
                 F("Erfolgreich Analog-Min/Max aktualisiert für ") + sensorId + F("[") +
                     String(measurementIndex) + F("]: min=") +
                     String(config.measurements[measurementIndex].minValue) + F(", max=") +
                     String(config.measurements[measurementIndex].maxValue));
  } else {
    logger.debug(F("AdminSensorHandler"),
                 F("Keine Änderungen für Analog-Min/Max-Werte festgestellt"));
  }

  sendJsonResponse(200, F("{\"success\":true}"));
#else
  sendJsonResponse(400, F("{\"success\":false,\"error\":\"Analog sensors not enabled\"}"));
#endif
}

void AdminSensorHandler::handleAnalogAutocal() {
#if USE_ANALOG
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    logger.warning(F("AdminSensorHandler"),
                   F("handleAnalogAutocal: Authentifizierung fehlgeschlagen"));
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  // Log at info level so operators can see toggles in the normal log level
  logger.info(F("AdminSensorHandler"), F("handleAnalogAutocal-Aufruf erhalten"));

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("enabled")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  bool enabled = _server.arg("enabled") == "true";

  logger.info(F("AdminSensorHandler"), F("handleAnalogAutocal: sensor=") + sensorId +
                                           F(", measurement=") + String(measurementIndex) +
                                           F(", enabled=") + String(enabled));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(500,
                     F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  if (!isAnalogSensor(sensor)) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  if (config.measurements[measurementIndex].calibrationMode == enabled) {
    sendJsonResponse(200, F("{\"success\":true}"));
    return;
  }

  // If enabling autocal, initialize runtime autocal from current calculation
  // limits and persist those limits BEFORE persisting the calibrationMode.
  if (enabled) {
    if (!isAnalogSensor(sensor)) {
      sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
      return;
    }
    AnalogSensor* analogSensor = static_cast<AnalogSensor*>(sensor);
    // Prefer initializing AutoCal from the last raw reading if available.
    // Only adjust the stored calculation limits when the last raw reading
    // lies outside of the currently configured valid range. If the
    // configured min/max are not valid (min>=max) then use the last raw
    // reading to anchor both limits (legacy/blank config case).
    uint16_t initMin = 0;
    uint16_t initMax = 1023;
    int lastRaw = -1;
    if (isAnalogSensor(sensor)) {
      AnalogSensor* tmpAnalog = static_cast<AnalogSensor*>(sensor);
      lastRaw = tmpAnalog->getLastRawValue(measurementIndex);
    }
    float cfgMinF = config.measurements[measurementIndex].minValue;
    float cfgMaxF = config.measurements[measurementIndex].maxValue;
    bool cfgValid = (cfgMinF < cfgMaxF);
    uint16_t cfgMin = static_cast<uint16_t>(constrain(cfgMinF, 0.0f, 1023.0f));
    uint16_t cfgMax = static_cast<uint16_t>(constrain(cfgMaxF, 0.0f, 1023.0f));

    if (lastRaw >= 0) {
      if (cfgValid) {
        // Only expand/contract the range when the last reading is outside
        // the configured range. Preserve the configured range otherwise.
        if (static_cast<uint16_t>(lastRaw) < cfgMin) {
          initMin = static_cast<uint16_t>(lastRaw);
          initMax = cfgMax;
        } else if (static_cast<uint16_t>(lastRaw) > cfgMax) {
          initMin = cfgMin;
          initMax = static_cast<uint16_t>(lastRaw);
        } else {
          // Last reading inside current configured range — keep it.
          initMin = cfgMin;
          initMax = cfgMax;
        }
      } else {
        // No valid configured range — anchor autocal to the observed value
        initMin = static_cast<uint16_t>(lastRaw);
        initMax = static_cast<uint16_t>(lastRaw);
      }
    } else {
      // No last raw reading — fall back to configured range or full ADC span
      if (cfgValid) {
        initMin = cfgMin;
        initMax = cfgMax;
      } else {
        initMin = 0;
        initMax = 1023;
      }
    }
    AutoCal cal;
    cal.min_value = initMin;
    cal.max_value = initMax;
    cal.min_value_f = static_cast<float>(initMin);
    cal.max_value_f = static_cast<float>(initMax);
    cal.last_update_time = 0;
    // Update in-memory config and runtime immediately
    config.measurements[measurementIndex].autocal = cal;
    analogSensor->setAutoCalibration(measurementIndex, cal);
    // Ensure runtime sees calibrationMode immediately to avoid transient
    // clamping while persistence/reload occurs.
    analogSensor->setCalibrationMode(measurementIndex, enabled);

    // Persist calibrationMode first so a reload does not temporarily
    // reset the runtime flag while we also write initial min/max.
    config.measurements[measurementIndex].calibrationMode = enabled;
    auto result =
        SensorPersistence::updateAnalogCalibrationMode(sensorId, measurementIndex, enabled);
    if (!result.isSuccess()) {
      logger.error(F("AdminSensorHandler"),
                   F("Fehler beim Aktivieren des Kalibrierungsmodus: ") + result.getMessage());
      sendJsonResponse(
          500, F("{\"success\":false,\"error\":\"Fehler beim Speichern des Kalibrierungsmodus\"}"));
      return;
    }

    // Now persist calculation limits (if needed). Doing this after
    // calibrationMode ensures reloads keep calibrationMode=true.
    bool needPersist = true;
    if (cfgValid && initMin == cfgMin && initMax == cfgMax)
      needPersist = false;
    if (needPersist) {
      // Persist rounded integer values to keep sensors.json format stable
      int persistMin = static_cast<int>(roundf(static_cast<float>(initMin)));
      int persistMax = static_cast<int>(roundf(static_cast<float>(initMax)));
      auto pm = SensorPersistence::updateAnalogMinMaxIntegerNoReload(
          sensorId, measurementIndex, persistMin, persistMax,
          config.measurements[measurementIndex].inverted);
      if (!pm.isSuccess()) {
        logger.warning(F("AdminSensorHandler"),
                       F("Konnte initiale Autocal-Min/Max nicht persistieren: ") + pm.getMessage());
        // continue — calibrationMode is persisted already and runtime set
      }
    } else {
      if (ConfigMgr.isDebugSensor()) {
        logger.debug(F("AdminSensorHandler"), F("Initiale Autocal-Min/Max entspricht vorhandener "
                                                "Konfiguration; Persistierung übersprungen"));
      }
    }

    // Wenn Autokalibrierung aktiviert wird und noch keine historischen
    // Roh-Extrema vorhanden sind (Sentinelwerte), initialisieren wir die
    // Extremwerte mit dem zuletzt gemessenen Rohwert, damit die UI nicht
    // weiterhin "--" anzeigt. Nur setzen, wenn die Extrema tatsächlich
    // noch unset sind; wir überschreiben damit keine vorhandene Historie.
    if (enabled) {
      int storedRawMin = config.measurements[measurementIndex].absoluteRawMin;
      int storedRawMax = config.measurements[measurementIndex].absoluteRawMax;
      if (storedRawMin == INT_MAX && storedRawMax == INT_MIN) {
        if (lastRaw >= 0) {
          // Update in-memory immediately so concurrent code sees values
          config.measurements[measurementIndex].absoluteRawMin = lastRaw;
          config.measurements[measurementIndex].absoluteRawMax = lastRaw;
          // Persist atomically and reload so runtime and UI are in sync
          auto rawPersist = SensorPersistence::updateAnalogRawMinMax(sensorId, measurementIndex,
                                                                     lastRaw, lastRaw);
          if (!rawPersist.isSuccess()) {
            logger.warning(F("AdminSensorHandler"),
                           F("Konnte initiale absolute Roh-Min/Max nicht persistieren: ") +
                               rawPersist.getMessage());
          } else {
            logger.info(F("AdminSensorHandler"), F("Initiale absolute Roh-Min/Max gesetzt für ") +
                                                     sensorId + F("[") + String(measurementIndex) +
                                                     F("]: ") + String(lastRaw));
          }
        } else {
          if (ConfigMgr.isDebugSensor()) {
            logger.debug(
                F("AdminSensorHandler"),
                F("Kein letzter Rohwert verfügbar, initiale absolute Roh-Extrema nicht gesetzt"));
          }
        }
      } else {
        if (ConfigMgr.isDebugSensor()) {
          logger.debug(F("AdminSensorHandler"),
                       F("Absolute Roh-Extrema bereits vorhanden, seeding uebersprungen"));
        }
      }
    }

    logger.info(F("AdminSensorHandler"), F("Autokalibrierung für ") + sensorId + F("[") +
                                             String(measurementIndex) +
                                             F("] aktiviert und initialisiert"));
    sendJsonResponse(200, F("{\"success\":true}"));
    return;
  }

  // Disabling autocal: persist the flag and reload
  config.measurements[measurementIndex].calibrationMode = enabled;
  // Also set runtime flag immediately so measurements stop autocalating
  // or resume clamping without waiting for reload.
  if (isAnalogSensor(sensor)) {
    AnalogSensor* analogSensor = static_cast<AnalogSensor*>(sensor);
    analogSensor->setCalibrationMode(measurementIndex, enabled);
  }
  auto result = SensorPersistence::updateAnalogCalibrationMode(sensorId, measurementIndex, enabled);
  if (!result.isSuccess()) {
    logger.error(F("AdminSensorHandler"),
                 F("Fehler beim Deaktivieren des Kalibrierungsmodus: ") + result.getMessage());
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Fehler beim Speichern des Kalibrierungsmodus\"}"));
    return;
  }
  logger.info(F("AdminSensorHandler"), F("Autokalibrierung für ") + sensorId + F("[") +
                                           String(measurementIndex) + F("] deaktiviert"));
  sendJsonResponse(200, F("{\"success\":true}"));
  return;
#else
  sendJsonResponse(400, F("{\"success\":false,\"error\":\"Analog sensors not enabled\"}"));
#endif
}

void AdminSensorHandler::handleAnalogAutocalDuration() {
#if USE_ANALOG
  if (!requireAjaxRequest())
    return;
  if (!validateRequest()) {
    sendJsonResponse(401, F("{\"success\":false,\"error\":\"Authentifizierung erforderlich\"}"));
    return;
  }

  if (!_server.hasArg("sensor_id") || !_server.hasArg("measurement_index") ||
      !_server.hasArg("duration")) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Erforderliche Parameter fehlen\"}"));
    return;
  }

  String sensorId = _server.arg("sensor_id");
  size_t measurementIndex = _server.arg("measurement_index").toInt();
  unsigned long dur = _server.arg("duration").toInt();

  logger.debug(F("AdminSensorHandler"), F("handleAnalogAutocalDuration: sensor=") + sensorId +
                                            F(", measurement=") + String(measurementIndex) +
                                            F(", duration=") + String(dur));

  if (!_sensorManager.isHealthy()) {
    sendJsonResponse(500,
                     F("{\"success\":false,\"error\":\"Sensor-Manager nicht betriebsbereit\"}"));
    return;
  }

  Sensor* sensor = _sensorManager.getSensor(sensorId);
  if (!sensor) {
    sendJsonResponse(404, F("{\"success\":false,\"error\":\"Sensor nicht gefunden\"}"));
    return;
  }

  if (!sensor->isInitialized()) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor nicht initialisiert\"}"));
    return;
  }

  if (!isAnalogSensor(sensor)) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Sensor ist nicht analog\"}"));
    return;
  }

  SensorConfig& config = sensor->mutableConfig();
  if (measurementIndex >= config.measurements.size()) {
    sendJsonResponse(400, F("{\"success\":false,\"error\":\"Ungültiger Messindex\"}"));
    return;
  }

  // Update runtime copy immediately
  config.measurements[measurementIndex].autocalHalfLifeSeconds = static_cast<uint32_t>(dur);
  // Persist atomically
  auto pres = SensorPersistence::updateAutocalDuration(sensorId, measurementIndex,
                                                       static_cast<uint32_t>(dur));
  if (!pres.isSuccess()) {
    logger.error(F("AdminSensorHandler"),
                 F("Fehler beim Persistieren der Autocal-Dauer: ") + pres.getMessage());
    sendJsonResponse(
        500, F("{\"success\":false,\"error\":\"Fehler beim Speichern der Autocal-Dauer\"}"));
    return;
  }

  logger.info(F("AdminSensorHandler"), F("Autocal-Dauer aktualisiert für ") + sensorId + F("[") +
                                           String(measurementIndex) + F("] -> ") + String(dur));
  sendJsonResponse(200, F("{\"success\":true}"));
#else
  sendJsonResponse(400, F("{\"success\":false,\"error\":\"Analog sensors not enabled\"}"));
#endif
}
