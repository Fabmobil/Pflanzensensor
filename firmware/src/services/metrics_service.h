/**
 * @file metrics_service.h
 * @brief Prometheus-Ausgabe /metrics (Namen kompatibel, refactor.md 3.2)
 */
#pragma once

#include "core/json_writer.h"
#include "services/config_service.h"
#include "services/net_service.h"
#include "services/sensor_service.h"
#include "services/system_info.h"

namespace ps {

class MetricsService {
public:
  MetricsService(ConfigService& cfg, SensorService& sensors, NetService& net, SystemInfo& sys)
      : cfg_(cfg), sensors_(sensors), net_(net), sys_(sys) {}
  void write(Sink& out);
  /// Label-Wert nach Prometheus-Regeln escapen (\\, \", \n)
  static void escapeLabel(Sink& out, const char* s);

private:
  ConfigService& cfg_;
  SensorService& sensors_;
  NetService& net_;
  SystemInfo& sys_;
};

} // namespace ps
