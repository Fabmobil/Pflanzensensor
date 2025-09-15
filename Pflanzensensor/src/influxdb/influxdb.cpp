#include "influxdb.h"
#include "configs/config.h"

#if USE_INFLUXDB

#include <InfluxDbClient.h>
#include <LittleFS.h>

#include <memory>

#include "logger/logger.h"
#include "utils/helper.h"
#include "utils/wifi.h"

std::unique_ptr<InfluxDBClient> influxclient = nullptr;

ResourceResult setupInfluxdb() {
  // Wait for actual time synchronization
  int retries = 0;
  const int MAX_TIME_SYNC_RETRIES = 3;

  while (retries < MAX_TIME_SYNC_RETRIES) {
    time_t currentTime = logger.getSynchronizedTime();
    if (currentTime > 24 * 3600) {  // Time is after 1970-01-02
  logger.info(F("InfluxDB"),
      String(F("Zeit erfolgreich synchronisiert: ")) + String(currentTime));
      break;
    }

  logger.debug(F("InfluxDB"), String(F("Warte auf Zeitsync, Versuch ")) +
                  String(retries + 1) + "/" +
                  String(MAX_TIME_SYNC_RETRIES));
    delay(1000);
    logger.updateNTP();  // Force an NTP update
    retries++;
  }

  if (logger.getSynchronizedTime() < 24 * 3600) {
  logger.error(F("InfluxDB"), String(F("Gültige Zeit nach ")) +
                  String(MAX_TIME_SYNC_RETRIES) +
                  String(F(" Versuchen nicht erhalten")));
    return ResourceResult::fail(ResourceError::TIME_SYNC_ERROR,
                F("Gültige Zeit nach ") +
                  String(MAX_TIME_SYNC_RETRIES) +
                  F(" Versuchen nicht erhalten"));
  }

  if (!influxclient) {
  logger.info(F("InfluxDB"), F("Initialisiere InfluxDB-Verbindung..."));
  logger.debug(F("InfluxDB"), String(F("InfluxDB URL: ")) + String(INFLUXDB_URL));

#if INFLUXDB_USE_V2
    // InfluxDB 2.x setup
  logger.debug(F("InfluxDB"), F("Benutze InfluxDB 2.x Konfiguration"));
    influxclient = std::make_unique<InfluxDBClient>(
        INFLUXDB_URL, INFLUXDB2_ORG, INFLUXDB2_BUCKET, INFLUXDB2_TOKEN);
#else
    // InfluxDB 1.x setup
  logger.debug(F("InfluxDB"), F("Benutze InfluxDB 1.x Konfiguration"));
    influxclient = std::make_unique<InfluxDBClient>();
    influxclient->setConnectionParamsV1(INFLUXDB_URL, INFLUXDB1_DB_NAME,
                                        INFLUXDB1_USER, INFLUXDB1_PASSWORD);

  logger.debug(F("InfluxDB"), String(F("Datenbank: ")) + String(INFLUXDB1_DB_NAME));
  logger.debug(F("InfluxDB"), String(F("Benutzer: ")) + String(INFLUXDB1_USER));
#endif

    // Configure client options
    influxclient->setWriteOptions(
        WriteOptions().writePrecision(WritePrecision::S));
    influxclient->setHTTPOptions(HTTPOptions().connectionReuse(true));
  }

  // Test connection with retry logic
  int connectionRetries = 0;
  const int MAX_CONNECTION_RETRIES = 3;

  while (connectionRetries < MAX_CONNECTION_RETRIES) {
    if (influxclient->validateConnection()) {
  logger.info(F("InfluxDB"), F("Erfolgreich mit InfluxDB verbunden"));
      return ResourceResult::success();
    }

    String lastError = influxclient->getLastErrorMessage();
  logger.warning(F("InfluxDB"), String(F("InfluxDB Verbindungsversuch ")) +
                    String(connectionRetries + 1) + "/" +
                    String(MAX_CONNECTION_RETRIES) +
                    String(F(" fehlgeschlagen: ")) + lastError);

    connectionRetries++;
    if (connectionRetries < MAX_CONNECTION_RETRIES) {
      delay(1000);  // Wait before retry
    }
  }

  String finalError = influxclient->getLastErrorMessage();
  logger.error(F("InfluxDB"), String(F("InfluxDB-Verbindung nach ")) +
                  String(MAX_CONNECTION_RETRIES) +
                  String(F(" Versuchen fehlgeschlagen: ")) + finalError);
  return ResourceResult::fail(ResourceError::INFLUXDB_ERROR,
                F("InfluxDB-Verbindung nach ") +
                  String(MAX_CONNECTION_RETRIES) +
                  F(" Versuchen fehlgeschlagen: ") + finalError);
}

ResourceResult influxdbSendSystemInfo() {
#if INFLUXDB_SYSTEMINFO_ACTIVE
  static unsigned long lastSystemInfoUpdate = 0;
  static unsigned long lastErrorLog = 0;
  static unsigned long lastReconnectAttempt = 0;
  const unsigned long ERROR_LOG_INTERVAL = 60000;  // 1 minute
  const unsigned long RECONNECT_INTERVAL = 30000;  // 30 seconds
  unsigned long currentTime = millis();

  if (currentTime - lastSystemInfoUpdate <
      INFLUXDB_SYSTEMINFO_INTERVAL * 60 * 1000) {
    return ResourceResult::success();
  }

  // Check memory before proceeding
  if (ESP.getFreeHeap() < 4000) {
  return ResourceResult::fail(ResourceError::INSUFFICIENT_MEMORY,
                F("Unzureichender Speicher für System-Infos"));
  }

  if (!influxclient || !influxclient->isConnected()) {
    // Try reconnecting if enough time has passed since last attempt
    if (currentTime - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      auto setupResult = setupInfluxdb();
      if (setupResult.isSuccess()) {
        lastReconnectAttempt = currentTime;
      } else {
        if (currentTime - lastErrorLog >= ERROR_LOG_INTERVAL) {
          lastErrorLog = currentTime;
        }
        lastReconnectAttempt = currentTime;
  return ResourceResult::fail(ResourceError::INFLUXDB_ERROR,
            F("Erneute Verbindung zu InfluxDB fehlgeschlagen"));
      }
    }
  return ResourceResult::fail(ResourceError::INFLUXDB_ERROR,
                F("InfluxDB nicht verbunden"));
  }

  Point measurement(INFLUXDB_MEASUREMENT_NAME);
  measurement.addTag("hostname", HOSTNAME);
  measurement.addTag("type", "system");

  // Add system metrics
  measurement.addField("free_heap", ESP.getFreeHeap());
  measurement.addField("uptime", currentTime / 1000);  // Convert to seconds
  measurement.addField("reboot_count", Helper::getRebootCount());
  measurement.addField("heap_fragmentation", ESP.getHeapFragmentation());
  measurement.addField("max_free_block", ESP.getMaxFreeBlockSize());
  measurement.addField("wifi_rssi", WiFi.RSSI());

  // Set time explicitly from NTP
  measurement.setTime(logger.getSynchronizedTime());

  // Write with minimal error logging
  if (!influxclient->writePoint(measurement)) {
    if (currentTime - lastErrorLog >= ERROR_LOG_INTERVAL) {
      lastErrorLog = currentTime;
    }
  return ResourceResult::fail(ResourceError::INFLUXDB_ERROR,
                F("Schreiben der System-Infos nach InfluxDB fehlgeschlagen"));
  }

  lastSystemInfoUpdate = currentTime;
  return ResourceResult::success();

#else
  return ResourceResult::success();
#endif
}

ResourceResult influxdbSendMeasurement(const Sensor* sensor,
                                       const MeasurementData& measurementData) {
#if USE_INFLUXDB
  if (!sensor || !influxclient) {
  logger.error(F("InfluxDB"), F("InfluxDB: Ungültiger Sensor oder Client"));
    return ResourceResult::fail(ResourceError::INFLUXDB_ERROR,
                F("Ungültiger Sensor oder InfluxDB-Client"));
  }

  // Ensure InfluxDB is properly initialized
  if (!influxclient->isConnected()) {
  logger.debug(F("InfluxDB"), F("InfluxDB nicht verbunden, versuche Einrichtung"));
    auto setupResult = setupInfluxdb();
    if (!setupResult.isSuccess()) {
      return ResourceResult::fail(ResourceError::INFLUXDB_ERROR,
                                  F("Einrichtung der InfluxDB-Verbindung fehlgeschlagen"));
    }
  }

  Point measurement(INFLUXDB_MEASUREMENT_NAME);
  measurement.addTag("hostname", HOSTNAME);
  measurement.addTag("sensor_id", sensor->getId());
  measurement.addTag("sensor_name", sensor->getName());
  measurement.addTag("status", sensor->getStatus());

  // Add timestamp to measurement
  measurement.setTime(logger.getSynchronizedTime());

  String measurementLog = String(F("Sende an InfluxDB [")) + sensor->getName() + "]:";
  bool hasValidData = false;

  // Clamp activeValues
  if (measurementData.activeValues > SensorConfig::MAX_MEASUREMENTS) {
  logger.warning(F("InfluxDB"), F("Begrenze activeValues von ") +
                                      String(measurementData.activeValues) +
                    F(" auf ") +
                                      String(SensorConfig::MAX_MEASUREMENTS));
    // Note: measurementData is const, so we can't modify it directly. Use a
    // local variable for safe access.
  }
  size_t safeActiveValues =
      std::min(measurementData.activeValues, SensorConfig::MAX_MEASUREMENTS);
  for (size_t i = 0; i < safeActiveValues; i++) {
    const auto& config = sensor->config();
    if (i >= config.activeMeasurements || !config.measurements[i].enabled) {
      continue;
    }

    String fieldName = measurementData.fieldNames[i];
    float value = measurementData.values[i];

    // Validate field name and provide fallback
    if (fieldName.isEmpty()) {
  logger.error(F("InfluxDB"), String(F("Leerer Feldname für Messung ")) +
              String(i) + F(", nutze Fallback"));
  fieldName = String(F("value_")) + String(i);
    }

    if (isnan(value) || !sensor->isValidValue(value, i)) {
      continue;
    }

    measurement.addField(fieldName, value);
    measurementLog += " " + fieldName + ": " + String(value) + " " +
                      String(measurementData.units[i]);
    hasValidData = true;
  }

  if (!hasValidData) {
  logger.warning(F("InfluxDB"),
           sensor->getName() + F(": Keine gültigen Daten zum Senden"));
    return ResourceResult::fail(
        ResourceError::VALIDATION_ERROR,
    sensor->getName() + F(": Keine gültigen Daten zum Senden"));
  }

  logger.debug(F("InfluxDB"), measurementLog);

  // Check connection status before writing
  if (!influxclient->isConnected()) {
  logger.error(
    F("InfluxDB"),
    sensor->getName() + F(": InfluxDB nicht verbunden, versuche erneut zu verbinden"));
    auto setupResult = setupInfluxdb();
    if (!setupResult.isSuccess()) {
      return ResourceResult::fail(
          ResourceError::INFLUXDB_ERROR,
          sensor->getName() + F(": Erneute Verbindung zu InfluxDB fehlgeschlagen"));
    }
  }

  if (influxclient->writePoint(measurement)) {
    return ResourceResult::success();
  }

  String lastError = influxclient->getLastErrorMessage();
  logger.error(
    F("InfluxDB"),
    sensor->getName() + F(": Schreiben nach InfluxDB fehlgeschlagen - ") + lastError);
  return ResourceResult::fail(
      ResourceError::INFLUXDB_ERROR,
    sensor->getName() + F(": Schreiben nach InfluxDB fehlgeschlagen - ") + lastError);

#else
  return ResourceResult::success();
#endif
}

#endif  // USE_INFLUXDB
