/**
 * @file catalog.cpp
 * @brief Tabellen der Sensortypen und Kanäle (Werte aus config_example.h des Altcodes)
 */
#include "core/sensors/catalog.h"

namespace ps {
namespace catalog {

namespace {

constexpr ChronikEncoding H = ChronikEncoding::Half;
constexpr ChronikEncoding I = ChronikEncoding::Int16Scaled;
constexpr ChronikEncoding F = ChronikEncoding::Float32;

const SensorInfo kInfo[kSensorTypeCount] PS_PROGMEM = {
    // id, Ressourcen, Kanäle, Intervall s, Start-Warmup, Prepare-Warmup, Rundenabstand, Rundenfrist, Runden
    {"ANALOG", ResAdc, 8, 60, 0, 0, 300, 200, 3},
    {"DHT", ResNone, 2, 60, 0, 0, 1000, 500, 3},
    {"DS18B20", ResOneWire, 8, 60, 0, 0, 0, 1500, 3},
    {"SDS011", ResSoftUart, 2, 600, 0, 30000, 1000, 1500, 3},
    {"MHZ19", ResNone, 1, 60, 180000, 0, 2000, 3000, 3},
    {"HX711", ResNone, 2, 60, 0, 0, 100, 2500, 3},
    {"BMP280", ResI2c, 2, 60, 0, 0, 100, 200, 3},
    {"SERIAL_RECEIVER", ResSoftUart, 7, 60, 0, 0, 0, 30000, 1},
};

#define LIM(a, b, c, d)                                                                            \
  Limits { a, b, c, d }

const ChannelDef kAnalog[8] PS_PROGMEM = {
    {"lichtstaerke", "Lichtstärke", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 5, 1023, false,
     H, 1, 0},
    {"bodenfeuchte", "Bodenfeuchte", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750, true,
     H, 1, 0},
    {"bodenfeuchte_2", "Bodenfeuchte 2", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750,
     true, H, 1, 0},
    {"bodenfeuchte_3", "Bodenfeuchte 3", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750,
     true, H, 1, 0},
    {"bodenfeuchte_4", "Bodenfeuchte 4", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750,
     true, H, 1, 0},
    {"bodenfeuchte_5", "Bodenfeuchte 5", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750,
     true, H, 1, 0},
    {"bodenfeuchte_6", "Bodenfeuchte 6", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750,
     true, H, 1, 0},
    {"bodenfeuchte_7", "Bodenfeuchte 7", "%", 0, 100, LIM(10, 20, 80, 90), false, true, 250, 750,
     true, H, 1, 0},
};

const ChannelDef kDht[2] PS_PROGMEM = {
    {"lufttemperatur", "Lufttemperatur", "°C", -40, 80, LIM(10, 15, 25, 30), false, false, 0, 0,
     false, H, 1, 0},
    {"luftfeuchte", "Luftfeuchte", "%", 0, 100, LIM(20, 30, 80, 90), false, false, 0, 0, false, H,
     1, 0},
};

const ChannelDef kDs18b20[8] PS_PROGMEM = {
    {"ds18b20_1", "DS18B20_1", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_2", "DS18B20_2", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_3", "DS18B20_3", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_4", "DS18B20_4", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_5", "DS18B20_5", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_6", "DS18B20_6", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_7", "DS18B20_7", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
    {"ds18b20_8", "DS18B20_8", "°C", -55, 125, LIM(-10, 0, 40, 60), false, false, 0, 0, false, H, 1,
     0},
};

// Einseitig: nur gh/yh zählen. yl/gl stehen auf 0, damit die Ordnung gilt.
const ChannelDef kSds011[2] PS_PROGMEM = {
    {"pm10", "PM10", "µg/m³", 0, 999.9f, LIM(0, 0, 25, 50), true, false, 0, 0, false, I, 0.1f, 0},
    {"pm25", "PM2.5", "µg/m³", 0, 999.9f, LIM(0, 0, 15, 35), true, false, 0, 0, false, I, 0.1f, 0},
};

const ChannelDef kMhz19[1] PS_PROGMEM = {
    {"co2", "CO2", "ppm", 0.5f, 5000, LIM(400, 600, 1200, 2000), false, false, 0, 0, false, I, 1,
     0},
};

// Kanal 1 rechnet der Sensordienst aus Kanal 0 und Nass-/Trockengewicht.
const ChannelDef kHx711[2] PS_PROGMEM = {
    {"gewicht", "Gewicht", "g", -1e7f, 1e7f, LIM(0, 10, 5000, 10000), false, false, 0, 0, false, F,
     1, 0},
    {"wasservorrat", "Wasservorrat", "%", 0, 100, LIM(10, 25, 100, 100), false, false, 0, 0, false,
     H, 1, 0},
};

const ChannelDef kBmp280[2] PS_PROGMEM = {
    {"bmp280_temperatur", "BMP_Temperatur", "°C", -40, 85, LIM(-10, 10, 35, 50), false, false, 0, 0,
     false, H, 1, 0},
    {"luftdruck", "Luftdruck", "hPa", 300, 1100, LIM(950, 980, 1030, 1060), false, false, 0, 0,
     false, I, 0.1f, 0},
};

const ChannelDef kSerial[7] PS_PROGMEM = {
    {"l_per_min", "Flow Rate", "l/min", 0, 1000, LIM(0, 0.1f, 100, 500), false, false, 0, 0, false,
     F, 1, 0},
    {"absolute_counts", "Absolute Counts", "counts", 0, 999999, LIM(0, 1, 999999, 999999), false,
     false, 0, 0, false, F, 1, 0},
    {"sum_l_per_min", "Sum Flow Rate", "l/min", 0, 999999, LIM(0, 0.1f, 999999, 999999), false,
     false, 0, 0, false, F, 1, 0},
    {"l_per_min_24h", "24h Flow Rate", "l/min", 0, 1000, LIM(0, 0.1f, 100, 500), false, false, 0, 0,
     false, F, 1, 0},
    {"arduino_millis", "Arduino Time", "ms", 0, 4294967295.0f,
     LIM(0, 0, 4294967295.0f, 4294967295.0f), false, false, 0, 0, false, F, 1, 0},
    {"uptime", "Uptime", "s", 0, 31536000, LIM(0, 0, 31536000, 31536000), false, false, 0, 0, false,
     F, 1, 0},
    {"l_per_hour", "Liters per Hour", "l/h", 0, 60000, LIM(0, 0.1f, 6000, 30000), false, false, 0,
     0, false, F, 1, 0},
};

#undef LIM

struct Table {
  const ChannelDef* defs;
  uint8_t count;
};

Table table(SensorType t) {
  switch (t) {
  case SensorType::Analog:
    return {kAnalog, 8};
  case SensorType::Dht:
    return {kDht, 2};
  case SensorType::Ds18b20:
    return {kDs18b20, 8};
  case SensorType::Sds011:
    return {kSds011, 2};
  case SensorType::Mhz19:
    return {kMhz19, 1};
  case SensorType::Hx711:
    return {kHx711, 2};
  case SensorType::Bmp280:
    return {kBmp280, 2};
  case SensorType::SerialReceiver:
    return {kSerial, 7};
  default:
    return {nullptr, 0};
  }
}

} // namespace

SensorInfo info(SensorType t) {
  SensorInfo out{};
  const uint8_t i = static_cast<uint8_t>(t);
  if (i < kSensorTypeCount)
    ps_memcpy_P(&out, &kInfo[i], sizeof(out));
  return out;
}

ChannelDef channel(SensorType t, uint8_t index) {
  ChannelDef out{};
  const Table tb = table(t);
  if (index < tb.count)
    ps_memcpy_P(&out, &tb.defs[index], sizeof(out));
  return out;
}

FStr id(SensorType t) {
  const uint8_t i = static_cast<uint8_t>(t);
  return FStr{i < kSensorTypeCount ? kInfo[i].id : kInfo[0].id};
}

SensorType typeFromId(const char* s, size_t len) {
  if (!s)
    return SensorType::Count;
  for (uint8_t i = 0; i < kSensorTypeCount; i++) {
    char buf[16];
    copyP(buf, sizeof(buf), FStr{kInfo[i].id});
    if (strlen(buf) == len && strncmp(buf, s, len) == 0)
      return static_cast<SensorType>(i);
  }
  return SensorType::Count;
}

void field(SensorType t, uint8_t index, char* out, size_t cap) {
  const Table tb = table(t);
  if (index >= tb.count) {
    if (cap)
      out[0] = '\0';
    return;
  }
  copyP(out, cap, FStr{tb.defs[index].field});
}

} // namespace catalog
} // namespace ps
