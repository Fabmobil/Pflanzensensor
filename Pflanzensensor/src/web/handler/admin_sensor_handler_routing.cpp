/**
 * @file admin_sensor_handler_routing.cpp
 * @brief Implementation of routing and basic request handling
 */

#include "admin_sensor_handler.h"
#include "logger/logger.h"

RouterResult AdminSensorHandler::onRegisterRoutes(WebRouter& router) {
  logger.debug(F("AdminSensorHandler"), F("Registriere Admin-Sensor-Routen"));

  auto result = router.addRoute(HTTP_GET, "/admin/sensors", [this]() {
  logger.debug(F("AdminSensorHandler"), F("GET /admin/sensors aufgerufen"));
    handleSensorConfig();
  });
  if (!result.isSuccess()) {
    logger.error(
        F("AdminSensorHandler"),
        F("Registrieren von GET /admin/sensors fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/sensors", [this]() {
  logger.debug(F("AdminSensorHandler"), F("POST /admin/sensors aufgerufen"));
    handleSensorUpdate();
  });
  if (!result.isSuccess()) {
  logger.error(
    F("AdminSensorHandler"),
    F("Registrieren von POST /admin/sensors fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/sensors/flower_status", [this]() {
  logger.debug(F("AdminSensorHandler"), F("POST /admin/sensors/flower_status aufgerufen"));
    std::map<String, String> params;
    for (int i = 0; i < _server.args(); i++) {
      params[_server.argName(i)] = _server.arg(i);
    }
    handleFlowerStatusUpdate(params);
  });
  if (!result.isSuccess()) {
  logger.error(
    F("AdminSensorHandler"),
    F("Registrieren von POST /admin/sensors/flower_status fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/sensor_update", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/sensor_update aufgerufen"));
    handleSingleSensorUpdate();
  });
  if (!result.isSuccess()) {
  logger.error(
    F("AdminSensorHandler"),
    F("Registrieren von POST /admin/sensor_update fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/measurement_interval", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/measurement_interval aufgerufen"));
    handleMeasurementInterval();
  });
  if (!result.isSuccess()) {
  logger.error(F("AdminSensorHandler"),
         F("Registrieren von POST /admin/measurement_interval fehlgeschlagen: ") +
           result.getMessage());
    return result;
  }

#if USE_ANALOG
  result = router.addRoute(HTTP_POST, "/admin/analog_minmax", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/analog_minmax aufgerufen"));
    handleAnalogMinMax();
  });
  if (!result.isSuccess()) {
  logger.error(
    F("AdminSensorHandler"),
    F("Registrieren von POST /admin/analog_minmax fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/analog_inverted", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/analog_inverted aufgerufen"));
    handleAnalogInverted();
  });
  if (!result.isSuccess()) {
    logger.error(F("AdminSensorHandler"),
                 F("Registrieren von POST /admin/analog_inverted fehlgeschlagen: ") +
                     result.getMessage());
    return result;
  }
#endif

  result = router.addRoute(HTTP_POST, "/admin/thresholds", [this]() {
  logger.debug(F("AdminSensorHandler"), F("POST /admin/thresholds aufgerufen"));
    handleThresholds();
  });
  if (!result.isSuccess()) {
  logger.error(
    F("AdminSensorHandler"),
    F("Registrieren von POST /admin/thresholds fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/measurement_name", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/measurement_name aufgerufen"));
    handleMeasurementName();
  });
  if (!result.isSuccess()) {
  logger.error(F("AdminSensorHandler"),
         F("Registrieren von POST /admin/measurement_name fehlgeschlagen: ") +
           result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/admin/reset_absolute_minmax", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/reset_absolute_minmax aufgerufen"));
    handleResetAbsoluteMinMax();
  });
  if (!result.isSuccess()) {
  logger.error(F("AdminSensorHandler"),
         F("Registrieren von POST /admin/reset_absolute_minmax fehlgeschlagen: ") +
           result.getMessage());
    return result;
  }

  result =
      router.addRoute(HTTP_POST, "/admin/reset_absolute_raw_minmax", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /admin/reset_absolute_raw_minmax aufgerufen"));
        handleResetAbsoluteRawMinMax();
      });
  if (!result.isSuccess()) {
  logger.error(F("AdminSensorHandler"),
         F("Registrieren von POST /admin/reset_absolute_raw_minmax fehlgeschlagen: ") +
           result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_POST, "/trigger_measurement", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("POST /trigger_measurement aufgerufen"));
    handleTriggerMeasurement();
  });
  if (!result.isSuccess()) {
  logger.error(
    F("AdminSensorHandler"),
    F("Registrieren von POST /trigger_measurement fehlgeschlagen: ") + result.getMessage());
    return result;
  }

  result = router.addRoute(HTTP_GET, "/admin/getSensorConfig", [this]() {
  logger.debug(F("AdminSensorHandler"),
         F("GET /admin/getSensorConfig aufgerufen"));
    handleGetSensorConfigJson();
  });
  if (!result.isSuccess()) {
  logger.error(F("AdminSensorHandler"),
         F("Registrieren von GET /admin/getSensorConfig fehlgeschlagen: ") +
           result.getMessage());
    return result;
  }

  logger.info(F("AdminSensorHandler"),
              F("Sensor-Config-Routen erfolgreich registriert"));
  return RouterResult::success();
}

HandlerResult AdminSensorHandler::handleGet(
    const String& uri, const std::map<String, String>& query) {
  if (uri == "/admin/sensors") {
    handleSensorConfig();
    return HandlerResult::success();
  }
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unbekannter Endpunkt");
}

HandlerResult AdminSensorHandler::handlePost(
    const String& uri, const std::map<String, String>& params) {
  if (uri == "/admin/sensors") {
    handleSensorUpdate();
    return HandlerResult::success();
  } else if (uri == "/trigger_measurement") {
    handleTriggerMeasurement();
    return HandlerResult::success();
  } else if (uri == "/admin/sensors/flower_status") {
    handleFlowerStatusUpdate(params);
    return HandlerResult::success();
  }
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unbekannter Endpunkt");
}
