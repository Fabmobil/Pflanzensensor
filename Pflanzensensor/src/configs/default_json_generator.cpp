#include "default_json_generator.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config_persistence.h"
#include "sensors/sensor_config.h"
#include "utils/persistence_utils.h"

void ensureConfigFilesExist() {
  // CONFIG
  if (!PersistenceUtils::fileExists("/config.json")) {
    ConfigData config;
    config.adminPassword = INITIAL_ADMIN_PASSWORD;
    config.md5Verification = false;
    config.collectdEnabled = USE_INFLUXDB;
    config.fileLoggingEnabled = FILE_LOGGING_ENABLED;
    config.debugRAM = DEBUG_RAM;
    config.debugMeasurementCycle = DEBUG_MEASUREMENT_CYCLE;
    config.debugSensor = DEBUG_SENSOR;
    config.debugDisplay = DEBUG_DISPLAY;
    config.debugWebSocket = DEBUG_WEBSOCKET;
    // Add WiFi credentials from config macros
    config.wifiSSID1 = WIFI_SSID_1;
    config.wifiPassword1 = WIFI_PASSWORD_1;
    config.wifiSSID2 = WIFI_SSID_2;
    config.wifiPassword2 = WIFI_PASSWORD_2;
    config.wifiSSID3 = WIFI_SSID_3;
    config.wifiPassword3 = WIFI_PASSWORD_3;
    // Add device name from macro
    config.deviceName = String(DEVICE_NAME);
    ConfigPersistence::saveToFileMinimal(config);
  logger.info(F("main"), F("/config.json mit Standardwerten beim Start erstellt."));
  }

  // SENSORS
  if (!PersistenceUtils::fileExists("/sensors.json")) {
    StaticJsonDocument<1048> doc;
    // DHT sensor (if enabled)
#if USE_DHT
    {
      auto dht = doc.createNestedObject("DHT");
      dht["name"] = DHT_TEMPERATURE_NAME;
      dht["measurementInterval"] = DHT_MEASUREMENT_INTERVAL * 1000UL;
      auto measurements = dht.createNestedObject("measurements");
      // Temperature
      auto temp = measurements.createNestedObject("0");
      temp["enabled"] = true;
      temp["min"] = -40.0f;
      temp["max"] = 80.0f;
      auto tempThresh = temp.createNestedObject("thresholds");
      tempThresh["yellowLow"] = DHT_TEMPERATURE_YELLOW_LOW;
      tempThresh["greenLow"] = DHT_TEMPERATURE_GREEN_LOW;
      tempThresh["greenHigh"] = DHT_TEMPERATURE_GREEN_HIGH;
      tempThresh["yellowHigh"] = DHT_TEMPERATURE_YELLOW_HIGH;
      // Humidity
      auto hum = measurements.createNestedObject("1");
      hum["enabled"] = true;
      hum["min"] = 1.0f;
      hum["max"] = 100.0f;
      auto humThresh = hum.createNestedObject("thresholds");
      humThresh["yellowLow"] = DHT_HUMIDITY_YELLOW_LOW;
      humThresh["greenLow"] = DHT_HUMIDITY_GREEN_LOW;
      humThresh["greenHigh"] = DHT_HUMIDITY_GREEN_HIGH;
      humThresh["yellowHigh"] = DHT_HUMIDITY_YELLOW_HIGH;
    }
#endif

    // Create unified DS18B20 sensor
#if USE_DS18B20
    auto ds18b20 = doc.createNestedObject("DS18B20");
    ds18b20["name"] = "DS18B20 Sensors";
    ds18b20["measurementInterval"] = DS18B20_MEASUREMENT_INTERVAL * 1000UL;
    auto ds18b20Measurements = ds18b20.createNestedObject("measurements");

    // Generate sensors based on DS18B20_SENSOR_DEFAULTS array
    for (size_t i = 0; i < getDS18B20SensorCount(); i++) {
      auto m = ds18b20Measurements.createNestedObject(String(i));
      m["name"] = String(DS18B20_SENSOR_DEFAULTS[i].name);
      m["enabled"] = true;
      m["min"] = DS18B20_MIN;
      m["max"] = DS18B20_MAX;
      auto t = m.createNestedObject("thresholds");
      t["yellowLow"] = DS18B20_SENSOR_DEFAULTS[i].yellowLow;
      t["greenLow"] = DS18B20_SENSOR_DEFAULTS[i].greenLow;
      t["greenHigh"] = DS18B20_SENSOR_DEFAULTS[i].greenHigh;
      t["yellowHigh"] = DS18B20_SENSOR_DEFAULTS[i].yellowHigh;
    }
#endif

    // SDS011 sensor (if enabled)
#if USE_SDS011
    {
      auto sds = doc.createNestedObject("SDS011");
      sds["name"] = SDS011_PM10_NAME;
      sds["measurementInterval"] = SDS011_MEASUREMENT_INTERVAL * 1000UL;
      auto measurements = sds.createNestedObject("measurements");
      // PM10
      auto pm10 = measurements.createNestedObject("0");
      pm10["enabled"] = true;
      pm10["min"] = 0.0f;
      pm10["max"] = 999.9f;
      auto pm10Thresh = pm10.createNestedObject("thresholds");
      pm10Thresh["greenHigh"] = SDS011_PM10_GREEN_HIGH;
      pm10Thresh["yellowHigh"] = SDS011_PM10_YELLOW_HIGH;
      // PM2.5
      auto pm25 = measurements.createNestedObject("1");
      pm25["enabled"] = true;
      pm25["min"] = 0.0f;
      pm25["max"] = 999.9f;
      auto pm25Thresh = pm25.createNestedObject("thresholds");
      pm25Thresh["greenHigh"] = SDS011_PM25_GREEN_HIGH;
      pm25Thresh["yellowHigh"] = SDS011_PM25_YELLOW_HIGH;
    }
#endif

    // MHZ19 sensor (if enabled)
#if USE_MHZ19
    {
      auto mhz = doc.createNestedObject("MHZ19");
      mhz["name"] = MHZ19_NAME;
      mhz["measurementInterval"] = MHZ19_MEASUREMENT_INTERVAL * 1000UL;
      auto measurements = mhz.createNestedObject("measurements");
      auto meas = measurements.createNestedObject("0");
      meas["enabled"] = true;
      meas["min"] = 1.0f;
      meas["max"] = 5000.0f;
      auto thresh = meas.createNestedObject("thresholds");
      thresh["yellowLow"] = MHZ19_YELLOW_LOW;
      thresh["greenLow"] = MHZ19_GREEN_LOW;
      thresh["greenHigh"] = MHZ19_GREEN_HIGH;
      thresh["yellowHigh"] = MHZ19_YELLOW_HIGH;
    }
#endif

    // Create unified ANALOG sensor
#if USE_ANALOG
    auto analog = doc.createNestedObject("ANALOG");
    analog["name"] = "Analog Sensors";
    analog["measurementInterval"] = ANALOG_MEASUREMENT_INTERVAL * 1000UL;
    auto analogMeasurements = analog.createNestedObject("measurements");

    // Generate sensors based on ANALOG_SENSOR_DEFAULTS array
    for (size_t i = 0; i < getAnalogSensorCount(); i++) {
      auto m = analogMeasurements.createNestedObject(String(i));
      m["name"] = String(ANALOG_SENSOR_DEFAULTS[i].name);
      m["enabled"] = true;
      m["min"] = ANALOG_SENSOR_DEFAULTS[i].rawMin;
      m["max"] = ANALOG_SENSOR_DEFAULTS[i].rawMax;
      m["inverted"] = false;  // Default to not inverted
      m["absoluteRawMin"] = 2147483647;
      m["absoluteRawMax"] = -2147483648;
      auto t = m.createNestedObject("thresholds");
      t["yellowLow"] = ANALOG_SENSOR_DEFAULTS[i].yellowLow;
      t["greenLow"] = ANALOG_SENSOR_DEFAULTS[i].greenLow;
      t["greenHigh"] = ANALOG_SENSOR_DEFAULTS[i].greenHigh;
      t["yellowHigh"] = ANALOG_SENSOR_DEFAULTS[i].yellowHigh;
    }
#endif

    // Serial Receiver sensor (if enabled)
#if USE_SERIAL_RECEIVER
    {
      auto serialReceiver = doc.createNestedObject("SERIAL_RECEIVER");
      serialReceiver["name"] = "Serial Receiver";
      serialReceiver["measurementInterval"] =
          SERIAL_RECEIVER_MEASUREMENT_INTERVAL * 1000UL;
      auto measurements = serialReceiver.createNestedObject("measurements");

      // Flow Rate (l/min)
      auto flowRate = measurements.createNestedObject("0");
      flowRate["name"] = "Flow Rate";
      flowRate["enabled"] = true;
      flowRate["min"] = 0.0f;
      flowRate["max"] = 1000.0f;
      flowRate["absoluteRawMin"] = 2147483647;
      flowRate["absoluteRawMax"] = -2147483648;
      auto flowRateThresh = flowRate.createNestedObject("thresholds");
      flowRateThresh["yellowLow"] = 0.0f;
      flowRateThresh["greenLow"] = 0.1f;
      flowRateThresh["greenHigh"] = 100.0f;
      flowRateThresh["yellowHigh"] = 500.0f;

      // Absolute Counts
      auto absCounts = measurements.createNestedObject("1");
      absCounts["name"] = "Absolute Counts";
      absCounts["enabled"] = true;
      absCounts["min"] = 0.0f;
      absCounts["max"] = 999999.0f;
      absCounts["absoluteRawMin"] = 2147483647;
      absCounts["absoluteRawMax"] = -2147483648;
      auto absCountsThresh = absCounts.createNestedObject("thresholds");
      absCountsThresh["yellowLow"] = 0.0f;
      absCountsThresh["greenLow"] = 1.0f;
      absCountsThresh["greenHigh"] = 999999.0f;
      absCountsThresh["yellowHigh"] = 999999.0f;

      // Total Flow
      auto totalFlow = measurements.createNestedObject("2");
      totalFlow["name"] = "Total Flow";
      totalFlow["enabled"] = true;
      totalFlow["min"] = 0.0f;
      totalFlow["max"] = 999999.0f;
      totalFlow["absoluteRawMin"] = 2147483647;
      totalFlow["absoluteRawMax"] = -2147483648;
      auto totalFlowThresh = totalFlow.createNestedObject("thresholds");
      totalFlowThresh["yellowLow"] = 0.0f;
      totalFlowThresh["greenLow"] = 0.1f;
      totalFlowThresh["greenHigh"] = 999999.0f;
      totalFlowThresh["yellowHigh"] = 999999.0f;

      // 24h Flow Rate
      auto flow24h = measurements.createNestedObject("3");
      flow24h["name"] = "24h Flow Rate";
      flow24h["enabled"] = true;
      flow24h["min"] = 0.0f;
      flow24h["max"] = 1000.0f;
      flow24h["absoluteRawMin"] = 2147483647;
      flow24h["absoluteRawMax"] = -2147483648;
      auto flow24hThresh = flow24h.createNestedObject("thresholds");
      flow24hThresh["yellowLow"] = 0.0f;
      flow24hThresh["greenLow"] = 0.1f;
      flow24hThresh["greenHigh"] = 100.0f;
      flow24hThresh["yellowHigh"] = 500.0f;

      // Arduino Millis
      auto arduinoMillis = measurements.createNestedObject("4");
      arduinoMillis["name"] = "Arduino Millis";
      arduinoMillis["enabled"] = true;
      arduinoMillis["min"] = 0.0f;
      arduinoMillis["max"] = 4294967295.0f;
      arduinoMillis["absoluteRawMin"] = 2147483647;
      arduinoMillis["absoluteRawMax"] = -2147483648;
      auto arduinoMillisThresh = arduinoMillis.createNestedObject("thresholds");
      arduinoMillisThresh["yellowLow"] = 0.0f;
      arduinoMillisThresh["greenLow"] = 0.0f;
      arduinoMillisThresh["greenHigh"] = 4294967295.0f;
      arduinoMillisThresh["yellowHigh"] = 4294967295.0f;

      // Uptime
      auto uptime = measurements.createNestedObject("5");
      uptime["name"] = "Uptime";
      uptime["enabled"] = true;
      uptime["min"] = 0.0f;
      uptime["max"] = 4294967295.0f;
      uptime["absoluteRawMin"] = 2147483647;
      uptime["absoluteRawMax"] = -2147483648;
      auto uptimeThresh = uptime.createNestedObject("thresholds");
      uptimeThresh["yellowLow"] = 0.0f;
      uptimeThresh["greenLow"] = 0.0f;
      uptimeThresh["greenHigh"] = 4294967295.0f;
      uptimeThresh["yellowHigh"] = 4294967295.0f;

      // Liters per Hour
      auto lPerHour = measurements.createNestedObject("6");
      lPerHour["name"] = "Liters per Hour";
      lPerHour["enabled"] = true;
      lPerHour["min"] = 0.0f;
      lPerHour["max"] = 60000.0f;
      lPerHour["absoluteRawMin"] = 2147483647;
      lPerHour["absoluteRawMax"] = -2147483648;
      auto lPerHourThresh = lPerHour.createNestedObject("thresholds");
      lPerHourThresh["yellowLow"] = 0.0f;
      lPerHourThresh["greenLow"] = 0.1f;
      lPerHourThresh["greenHigh"] = 6000.0f;
      lPerHourThresh["yellowHigh"] = 30000.0f;
    }
#endif
    String errorMsg;
    PersistenceUtils::writeJsonFile("/sensors.json", doc, errorMsg);
  logger.info(F("main"), F("/sensors.json mit Standardwerten beim Start erstellt."));
  }
}
