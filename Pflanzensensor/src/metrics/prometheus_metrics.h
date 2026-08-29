/**
 * @file prometheus_metrics.h
 * @brief Lightweight Prometheus metrics exporter for ESP8266
 * @details Implements a memory-efficient metrics system that exports
 *          data in Prometheus text format. No external dependencies required.
 */

#ifndef PROMETHEUS_METRICS_H
#define PROMETHEUS_METRICS_H

#include <Arduino.h>
#include <functional>
#include <vector>

#include "logger/logger.h"

// Configuration
#define PROMETHEUS_METRICS_ENABLED 1
#define PROMETHEUS_METRICS_PATH "/metrics"

/**
 * @class PrometheusMetrics
 * @brief Lightweight Prometheus metrics exporter
 * @details Memory-efficient implementation for ESP8266:
 *          - Custom counter, gauge, and help/type metadata
 *          - Thread-safe with critical sections
 *          - Prometheus text format output
 *          - Dynamic label support for sensor_id, sensor_type
 */
class PrometheusMetrics {
public:
  /**
   * @brief Singleton instance access
   * @return Reference to PrometheusMetrics instance
   */
  static PrometheusMetrics& getInstance();

  /**
   * @brief Initialize metrics system
   * @details Called during system initialization
   */
  void begin();

  /**
   * @brief Shutdown metrics system
   */
  void end();

  /**
   * @brief Gauge-Metriken entfernen (Counter bleiben erhalten)
   * @details Verhindert unbegrenztes Wachstum des Metrik-Vektors.
   *          Wird vor jedem Scrape aufgerufen, da Gauges ohnehin
   *          bei jedem Scrape neu berechnet werden.
   */
  void clearGauges();

  // Counter API
  /**
   * @brief Record a counter value
   * @param name Metric name
   * @param help Description text
   * @param value Value to record
   * @details Increments counter by value
   */
  void counterInc(const char* name, const char* help, const char* value = "1");

  /**
   * @brief Record a counter with labels
   * @param name Metric name
   * @param help Description text
   * @param labels Comma-separated label pairs (e.g., "sensor_id=1,sensor_type=temp")
   * @param value Value to record
   */
  void counterIncWithLabels(const char* name, const char* help, const char* labels,
                            const char* value = "1");

  // Gauge API
  /**
   * @brief Set a gauge value
   * @param name Metric name
   * @param help Description text
   * @param value Value to set
   */
  void gaugeSet(const char* name, const char* help, float value);

  /**
   * @brief Set a gauge value with labels
   * @param name Metric name
   * @param help Description text
   * @param labels Comma-separated label pairs
   * @param value Value to set
   */
  void gaugeSetWithLabels(const char* name, const char* help, const char* labels, float value);

  /**
   * @brief Increment gauge
   * @param name Metric name
   * @param help Description text
   * @param labels Comma-separated label pairs
   * @param value Amount to increment
   */
  void gaugeInc(const char* name, const char* help, const char* labels, float value);

  // Export
  /**
   * @brief Export all metrics in Prometheus text format
   * @return String containing all metrics
   * @details Generates properly formatted Prometheus output
   */
  String exportMetrics();

private:
  PrometheusMetrics() = default;
  ~PrometheusMetrics() = default;
  PrometheusMetrics(const PrometheusMetrics&) = delete;
  PrometheusMetrics& operator=(const PrometheusMetrics&) = delete;

  // Metric storage structures
  struct MetricEntry {
    String name;
    String help;
    String labels;
    float value;
    bool isCounter;
  };

  std::vector<MetricEntry> _metrics;
  bool _initialized{false};

  /**
   * @brief Escape string for Prometheus format
   * @param str String to escape
   * @return Escaped string
   */
  static String escapeString(const String& str);

  /**
   * @brief Add a new metric entry
   * @param name Metric name
   * @param help Description
   * @param labels Labels
   * @param value Value
   * @param isCounter Whether this is a counter
   * @return Index of added metric
   */
  size_t addMetric(const char* name, const char* help, const char* labels, float value,
                   bool isCounter);

  /**
   * @brief Find existing metric entry
   * @param name Metric name
   * @param labels Labels
   * @return Index of found metric or -1
   */
  int findMetric(const char* name, const char* labels);
};

#endif // PROMETHEUS_METRICS_H