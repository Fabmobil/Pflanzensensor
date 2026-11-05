/**
 * @file defaults.cpp
 * @brief Standardwerte (eine Stelle; Kanalwerte kommen aus dem Katalog)
 */
#include "core/config/defaults.h"

#if defined(PS_DEV_SECRETS)
#include "secrets.h" // nicht versioniert, nur für Entwickler
#endif

#include "core/config/schema.h"

namespace ps {
namespace defaults {

namespace {
bool isKnown(const char* key) { return schema::isChannelKey(key); }
} // namespace

void device(DeviceConfig& c) {
  c = DeviceConfig{};
  c.deviceName = kDeviceName;
  c.flowerChannel = isKnown(kFlowerChannel) ? kFlowerChannel : "";
  c.ledChannel = isKnown(kLedChannel) ? kLedChannel : "";
  c.mailUrl = kMailUrl;
  if (!board::features().display && c.ledMode == 1)
    c.ledMode = 2;
#if defined(PS_DEV_SECRETS)
  c.wifiSsid[0] = DEV_WIFI_SSID_1;
  c.wifiSsid[1] = DEV_WIFI_SSID_2;
  c.wifiSsid[2] = DEV_WIFI_SSID_3;
#endif
}

void secrets(Secrets& s) {
  s = Secrets{};
#if defined(PS_DEV_SECRETS)
  s.wifiPassword[0] = DEV_WIFI_PASSWORD_1;
  s.wifiPassword[1] = DEV_WIFI_PASSWORD_2;
  s.wifiPassword[2] = DEV_WIFI_PASSWORD_3;
#endif
}

} // namespace defaults

void sensorDefaults(SensorsConfig& c) {
  for (uint8_t i = 0; i < board::sensorCount(); i++) {
    const SensorInfo inf = catalog::info(board::sensor(i).type);
    c.sensors[i] = SensorConfig{};
    c.sensors[i].intervalS = inf.defaultIntervalS;
  }
  for (uint8_t s = 0; s < board::slotCount(); s++) {
    const auto& slot = board::slot(s);
    const ChannelDef d = catalog::channel(board::sensor(slot.sensor).type, slot.channel);
    ChannelConfig& ch = c.channels[s];
    ch = ChannelConfig{};
    ch.name = d.name;
    ch.unit = d.unit;
    ch.field = d.field;
    ch.limits = d.limits;
    if (d.hasRaw) {
      ch.rawMin = d.rawMin;
      ch.rawMax = d.rawMax;
      ch.inverted = d.inverted;
    }
  }
}

} // namespace ps
