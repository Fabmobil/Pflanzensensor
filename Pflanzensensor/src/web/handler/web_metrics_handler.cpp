/**
 * @file web_metrics_handler.cpp
 * @brief Prometheus metrics HTTP handler implementation
 */

#include "web/handler/web_metrics_handler.h"

#include "configs/config.h"
#include "managers/manager_config.h"
#include "metrics/prometheus_metrics.h"
#include "utils/helper.h"

WebMetricsHandler::WebMetricsHandler() : Manager("WebMetricsHandler") {}

void WebMetricsHandler::setSensorManager(SensorManager& sensorManager) {
  _sensorManager = &sensorManager;
}

void WebMetricsHandler::setActiveConnections(uint8_t count) { _activeConnections = count; }

void WebMetricsHandler::incrementRequestCounter(const char* handlerName, int statusCode) {
  String key = String(handlerName) + "_" + String(statusCode);
  _requestCounts[key]++;
}

String WebMetricsHandler::handleMetrics() {
#if !USE_PROMETHEUS_METRICS
  return String();
#else
  return collectMetrics();
#endif
}

String WebMetricsHandler::collectMetrics() {
  PrometheusMetrics& metrics = PrometheusMetrics::getInstance();

  // WICHTIG: Gauge-Metriken vor jedem Scrape zurücksetzen, damit keine
  // veralteten Einträge akkumulieren (Memory Leak auf ESP8266!)
  // Counter bleiben erhalten (kumulativ per Prometheus-Konvention).
  metrics.clearGauges();

  // Gerätename aus Konfiguration
  String deviceName = ConfigMgr.getDeviceName();
  if (deviceName == "") {
    deviceName = "unknown";
  }

  // System-Metriken sammeln (Speicher, WiFi, Uptime)
  collectSystemMetrics(deviceName);

  // Sensor-Metriken sammeln (falls SensorManager verfügbar)
  if (_sensorManager) {
    collectSensorMetrics(deviceName);
  }

  // Web-Server-Metriken (Request-Zähler)
  for (const auto& pair : _requestCounts) {
    metrics.counterIncWithLabels("pflanzenserver_requests_total",
                                 "Total HTTP requests by handler and status", pair.first.c_str(),
                                 String(pair.second).c_str());
  }

  metrics.gaugeSet("pflanzenserver_active_connections", "Active WebSocket connections",
                   _activeConnections);

  return metrics.exportMetrics();
}

void WebMetricsHandler::collectSystemMetrics(String deviceName) {
  PrometheusMetrics& metrics = PrometheusMetrics::getInstance();

  // Uptime in seconds
  unsigned long uptimeSec = millis() / 1000;

  // Create common labels for this device
  String labels = String("device=\"") + deviceName + "\",version=\"" + String(VERSION) +
                  "\",platform=\"ESP8266\",board=\"nodemcuv2\"";

  // Memory metrics
  metrics.gaugeSetWithLabels("pflanzensensor_memory_free_bytes", "Free heap memory", labels.c_str(),
                             (float)ESP.getFreeHeap());
#ifdef ESP32
  metrics.gaugeSetWithLabels("pflanzensensor_memory_max_block_bytes",
                             "Largest contiguous free block", labels.c_str(),
                             (float)ESP.getMaxAllocHeap());
  uint32_t heapFrag =
      ESP.getFreeHeap() > 0 ? (100 - (ESP.getMaxAllocHeap() * 100 / ESP.getFreeHeap())) : 0;
  metrics.gaugeSetWithLabels("pflanzensensor_memory_fragmentation_percent", "Heap fragmentation %",
                             labels.c_str(), (float)heapFrag);
#else
  metrics.gaugeSetWithLabels("pflanzensensor_memory_max_block_bytes",
                             "Largest contiguous free block", labels.c_str(),
                             (float)ESP.getMaxFreeBlockSize());
  metrics.gaugeSetWithLabels("pflanzensensor_memory_fragmentation_percent", "Heap fragmentation %",
                             labels.c_str(), (float)ESP.getHeapFragmentation());
#endif
  metrics.gaugeSetWithLabels("pflanzensensor_uptime_seconds", "Seconds since boot", labels.c_str(),
                             (float)uptimeSec);

  // WiFi metrics
  int8_t rssi = WiFi.RSSI();
  metrics.gaugeSetWithLabels("pflanzensensor_wifi_rssi_dbm", "WiFi signal strength in dBm",
                             labels.c_str(), (float)rssi);
  int wifiStatus = WiFi.status() == WL_CONNECTED ? 1 : 0;
  metrics.gaugeSetWithLabels("pflanzensensor_wifi_connected",
                             "WiFi connection status (0=disconnected, 1=connected)", labels.c_str(),
                             (float)wifiStatus);

  // Reboot count
  metrics.gaugeSetWithLabels("pflanzensensor_reboot_count_total", "Total number of device reboots",
                             labels.c_str(), (float)Helper::getRebootCount());
}

void WebMetricsHandler::collectSensorMetrics(String deviceName) {
  PrometheusMetrics& metrics = PrometheusMetrics::getInstance();

  // Get sensor data from sensor manager
  const auto& sensors = _sensorManager->getSensors();

  for (const auto& sensor : sensors) {
    if (!sensor || !sensor->isInitialized()) {
      continue;
    }

    if (sensor->getMeasurementStartTime() == 0) {
      continue;
    }

    const String& sensorId = sensor->getId();
    const auto& data = sensor->getMeasurementData();
    if (!data.isValid()) {
      continue;
    }

    // Export each measurement value with labels
    for (size_t i = 0; i < data.activeValues; ++i) {
      String fieldName = data.fieldNames[i];
      String unit = data.units[i];

      // 0 ppm is not a valid sensor reading (sensor not ready / no data)
      if (data.values[i] == 0.0f && unit == "ppm") {
        continue;
      }

      String labels = String("device=\"") + deviceName + "\",sensor_id=\"" + sensorId +
                      "\",field=\"" + fieldName + "\",unit=\"" + unit + "\"";

      metrics.gaugeSetWithLabels("pflanzensensor_sensor_value", "Current sensor reading",
                                 labels.c_str(), data.values[i]);
    }
  }
}
