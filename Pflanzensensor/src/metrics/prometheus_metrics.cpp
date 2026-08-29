/**
 * @file prometheus_metrics.cpp
 * @brief Prometheus metrics implementation for ESP8266
 */

#include "metrics/prometheus_metrics.h"
#include "utils/critical_section.h"
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

PrometheusMetrics& PrometheusMetrics::getInstance() {
  static PrometheusMetrics instance;
  return instance;
}

void PrometheusMetrics::begin() {
  CriticalSection cs;
  if (!_initialized) {
    _initialized = true;
    logger.info(F("PromMetrics"), F("Prometheus Metrics exporter initialized"));
  }
}

void PrometheusMetrics::end() {
  CriticalSection cs;
  _metrics.clear();
  _initialized = false;
}

void PrometheusMetrics::clearGauges() {
  CriticalSection cs;
  // Nur Gauge-Metriken entfernen, Counter behalten
  auto it = _metrics.begin();
  while (it != _metrics.end()) {
    if (!it->isCounter) {
      it = _metrics.erase(it);
    } else {
      ++it;
    }
  }
}

String PrometheusMetrics::escapeString(const String& str) {
  String escaped;
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (c == '\\' || c == '"') {
      escaped += '\\';
    }
    escaped += c;
  }
  return escaped;
}

size_t PrometheusMetrics::addMetric(const char* name, const char* help, const char* labels,
                                    float value, bool isCounter) {
  MetricEntry entry;
  entry.name = name;
  entry.help = help;
  entry.labels = labels ? labels : "";
  entry.value = value;
  entry.isCounter = isCounter;
  _metrics.push_back(entry);
  return _metrics.size() - 1;
}

int PrometheusMetrics::findMetric(const char* name, const char* labels) {
  String labelStr = labels ? labels : "";
  for (size_t i = 0; i < _metrics.size(); i++) {
    if (_metrics[i].name == name && _metrics[i].labels == labelStr) {
      return i;
    }
  }
  return -1;
}

void PrometheusMetrics::counterInc(const char* name, const char* help, const char* value) {
  CriticalSection cs;
  int idx = findMetric(name, "");
  if (idx >= 0) {
    _metrics[idx].value += atof(value);
  } else {
    addMetric(name, help, "", atof(value), true);
  }
}

void PrometheusMetrics::counterIncWithLabels(const char* name, const char* help, const char* labels,
                                             const char* value) {
  CriticalSection cs;
  int idx = findMetric(name, labels);
  if (idx >= 0) {
    _metrics[idx].value += atof(value);
  } else {
    addMetric(name, help, labels, atof(value), true);
  }
}

void PrometheusMetrics::gaugeSet(const char* name, const char* help, float value) {
  CriticalSection cs;
  int idx = findMetric(name, "");
  if (idx >= 0) {
    _metrics[idx].value = value;
  } else {
    addMetric(name, help, "", value, false);
  }
}

void PrometheusMetrics::gaugeSetWithLabels(const char* name, const char* help, const char* labels,
                                           float value) {
  CriticalSection cs;
  int idx = findMetric(name, labels);
  if (idx >= 0) {
    _metrics[idx].value = value;
  } else {
    addMetric(name, help, labels, value, false);
  }
}

void PrometheusMetrics::gaugeInc(const char* name, const char* help, const char* labels,
                                 float value) {
  CriticalSection cs;
  int idx = findMetric(name, labels);
  if (idx >= 0) {
    _metrics[idx].value += value;
  } else {
    addMetric(name, help, labels, value, false);
  }
}

String PrometheusMetrics::exportMetrics() {
  // WICHTIG: Metriken in lokale Kopie übernehmen, damit der CriticalSection
  // (Interrupt-Sperre) nur minimal gehalten wird. Die String-Formatierung
  // darf NICHT mit gesperrten Interrupts laufen — sonst Watchdog-Reset!
  std::vector<MetricEntry> metricsCopy;
  {
    CriticalSection cs;
    metricsCopy = _metrics;
  }

  // Bereits ausgegebene Metrik-Namen (für HELP/TYPE Deduplizierung)
  // Prometheus erlaubt nur EINE HELP/TYPE-Zeile pro Metrikname
  String output;
  String lastMetricName;

  for (const auto& metric : metricsCopy) {
    // HELP und TYPE nur beim ersten Vorkommen eines Metriknamens ausgeben
    if (metric.name != lastMetricName) {
      output += "# HELP ";
      output += metric.name;
      output += " ";
      output += escapeString(metric.help);
      output += "\n";

      output += "# TYPE ";
      output += metric.name;
      output += " ";
      output += metric.isCounter ? "counter" : "gauge";
      output += "\n";

      lastMetricName = metric.name;
    }

    // Metrikwert ausgeben
    output += metric.name;

    if (!metric.labels.isEmpty()) {
      output += "{";
      output += metric.labels;
      output += "}";
    }

    output += " ";
    if (metric.value == floor(metric.value)) {
      output += String((long)metric.value);
    } else {
      output += String(metric.value, 2);
    }
    output += "\n";

    yield(); // Watchdog füttern bei vielen Metriken
  }

  return output;
}
