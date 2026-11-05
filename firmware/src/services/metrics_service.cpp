/**
 * @file metrics_service.cpp
 */
#include "services/metrics_service.h"

#include <stdio.h>

#include "core/version.h"
#include "hal/system.h"

namespace ps {

void MetricsService::escapeLabel(Sink& out, const char* s) {
  for (; *s; s++) {
    if (*s == '\\')
      out.write("\\\\", 2);
    else if (*s == '"')
      out.write("\\\"", 2);
    else if (*s == '\n')
      out.write("\\n", 2);
    else
      out.write(s, 1);
  }
}

void MetricsService::write(Sink& out) {
  const char* dev = cfg_.device().deviceName.c_str();
  auto labels = [&]() {
    out.write("{device=\"");
    escapeLabel(out, dev);
    out.write("\",version=\"");
    escapeLabel(out, kVersion);
    out.write("\",platform=\"");
    escapeLabel(out, board::kPlatformName);
    out.write("\",board=\"");
    escapeLabel(out, board::name());
    out.write("\"}");
  };
  auto gauge = [&](FStr name, FStr help, double value) {
    char line[48];
    out.writeP(FS_("# HELP "));
    out.writeP(name);
    out.write(" ", 1);
    out.writeP(help);
    out.writeP(FS_("\n# TYPE "));
    out.writeP(name);
    out.writeP(FS_(" gauge\n"));
    out.writeP(name);
    labels();
    snprintf(line, sizeof(line), " %.0f\n", value);
    out.write(line);
  };
  const hal::HeapInfo& h = sys_.heap();
  gauge(FS_("pflanzensensor_memory_free_bytes"), FS_("Free heap memory in bytes"), h.free);
  gauge(FS_("pflanzensensor_memory_max_block_bytes"), FS_("Largest free memory block in bytes"),
        h.maxBlock);
  gauge(FS_("pflanzensensor_memory_fragmentation_percent"), FS_("Heap fragmentation percentage"),
        h.fragmentation);
  gauge(FS_("pflanzensensor_uptime_seconds"), FS_("Device uptime in seconds"), sys_.uptimeS());
  gauge(FS_("pflanzensensor_wifi_rssi_dbm"), FS_("WiFi signal strength in dBm"), net_.rssi());
  gauge(FS_("pflanzensensor_wifi_connected"),
        FS_("WiFi connection status (0=disconnected, 1=connected)"), net_.connected() ? 1 : 0);
  gauge(FS_("pflanzensensor_reboot_count_total"), FS_("Total number of device reboots"),
        sys_.rebootCount());

  out.writeP(FS_("# HELP pflanzensensor_sensor_value Current sensor reading\n# TYPE "
                 "pflanzensensor_sensor_value gauge\n"));
  for (uint8_t s = 0; s < board::slotCount(); s++) {
    const ChannelConfig& cc = cfg_.sensors().channels[s];
    if (!cc.enabled || !cfg_.sensors().sensors[board::slot(s).sensor].enabled)
      continue;
    const ChannelState& st = sensors_.channel(s);
    if (!st.hasOk || isnan(st.value) || isinf(st.value))
      continue;
    const SensorType t = board::sensor(board::slot(s).sensor).type;
    if (t == SensorType::Mhz19 && st.value == 0)
      continue;  // ppm == 0 übersprungen
    char id[24]; // Sensor-ID wie im Altcode ("ANALOG"); field trennt die Kanäle
    copyP(id, sizeof(id), catalog::id(t));
    out.writeP(FS_("pflanzensensor_sensor_value{device=\""));
    escapeLabel(out, dev);
    out.write("\",sensor_id=\"");
    escapeLabel(out, id);
    out.write("\",field=\"");
    escapeLabel(out, cc.field.c_str());
    out.write("\",unit=\"");
    escapeLabel(out, cc.unit.c_str());
    out.write("\"} ");
    char v[24];
    formatFloat(v, sizeof(v), st.value, 2);
    out.write(v);
    out.write("\n");
  }
}

} // namespace ps
