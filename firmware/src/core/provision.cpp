/**
 * @file provision.cpp
 */
#include "core/provision.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

#include "core/util/crc.h"

// JSON-Schlüssel im Flash (ESP8266: String-Literale lägen sonst im RAM)
#if defined(ARDUINO)
#include <Arduino.h>
#define PS_JK(s) F(s)
#else
#define PS_JK(s) s
#endif

namespace ps {
namespace provision {

namespace {

uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

void putLe32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = static_cast<uint8_t>(v >> (8 * i));
}

template <typename K, typename T> void readInt(JsonObjectConst o, K k, T& v) {
  if (o[k].template is<int>())
    v = static_cast<T>(o[k].template as<int>());
}
template <typename K> void readBool(JsonObjectConst o, K k, bool& v) {
  if (o[k].template is<bool>())
    v = o[k].template as<bool>();
}
void copyStr(JsonVariantConst v, char* out, size_t cap) {
  const char* s = v.as<const char*>();
  out[0] = '\0';
  if (s)
    snprintf(out, cap, "%s", s);
}
template <typename K> bool readIp(JsonObjectConst o, K k, uint8_t ip[4]) {
  const char* s = o[k].template as<const char*>();
  if (!s)
    return false;
  unsigned a, b, c, d;
  char tail;
  if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 ||
      d > 255)
    return false;
  ip[0] = static_cast<uint8_t>(a);
  ip[1] = static_cast<uint8_t>(b);
  ip[2] = static_cast<uint8_t>(c);
  ip[3] = static_cast<uint8_t>(d);
  return true;
}
template <typename K> void writeIp(JsonObject o, K k, const uint8_t ip[4]) {
  char s[16];
  snprintf(s, sizeof(s), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  o[k] = s;
}

// clang-format off
#define PS_PIN_FIELDS(X) \
  X(adc) \
  X(muxA) \
  X(muxB) \
  X(muxC) \
  X(dht) \
  X(ledGreen) \
  X(ledYellow) \
  X(ledRed) \
  X(sda) \
  X(scl) \
  X(oneWire) \
  X(sdsRx) \
  X(sdsTx) \
  X(mhz19Pwm) \
  X(hxSck) \
  X(hxDout) \
  X(serialRx) \
  X(serialTx)

#define PS_BOOL_FEATURES(X) \
  X(analog) \
  X(multiplexer) \
  X(dht) \
  X(ds18b20) \
  X(sds011) \
  X(mhz19) \
  X(hx711) \
  X(bmp280) \
  X(serialReceiver) \
  X(display) \
  X(ledTraffic)

#define PS_INT_FEATURES(X) \
  X(analogChannels) \
  X(ds18b20Max) \
  X(mhz19Range) \
  X(bmp280Address) \
  X(serialBaud) \
  X(serialTimeoutMs) \
  X(displayAddress)
// clang-format on

void readHardware(JsonObjectConst h, Hardware& hw) {
  if (h[PS_JK("name")].is<const char*>())
    copyStr(h[PS_JK("name")], hw.name, sizeof(hw.name));
  JsonObjectConst f = h[PS_JK("features")];
  Features& ft = hw.features;
#define X(n) readBool(f, PS_JK(#n), ft.n);
  PS_BOOL_FEATURES(X)
#undef X
#define X(n) readInt(f, PS_JK(#n), ft.n);
  PS_INT_FEATURES(X)
#undef X
  if (f[PS_JK("dhtType")].is<int>())
    ft.dhtType = f[PS_JK("dhtType")].as<int>() == 22 ? DhtType::Dht22 : DhtType::Dht11;
  JsonObjectConst p = h[PS_JK("pins")];
#define X(n) readInt(p, PS_JK(#n), hw.pins.n);
  PS_PIN_FIELDS(X)
#undef X
  JsonObjectConst ip = h[PS_JK("staticIp")];
  if (!ip.isNull()) {
    StaticIp s;
    s.enabled = readIp(ip, PS_JK("ip"), s.ip) && readIp(ip, PS_JK("gateway"), s.gateway);
    readIp(ip, PS_JK("subnet"), s.subnet);
    readIp(ip, PS_JK("dns1"), s.dns1);
    readIp(ip, PS_JK("dns2"), s.dns2);
    hw.staticIp = s;
  }
}

void writeHardware(JsonObject h, const Hardware& hw) {
  h[PS_JK("name")] = hw.name;
  JsonObject f = h[PS_JK("features")].to<JsonObject>();
  const Features& ft = hw.features;
#define X(n) f[PS_JK(#n)] = ft.n;
  PS_BOOL_FEATURES(X)
  PS_INT_FEATURES(X)
#undef X
  f[PS_JK("dhtType")] = static_cast<int>(ft.dhtType);
  JsonObject p = h[PS_JK("pins")].to<JsonObject>();
#define X(n) p[PS_JK(#n)] = hw.pins.n;
  PS_PIN_FIELDS(X)
#undef X
  if (hw.staticIp.enabled) {
    JsonObject ip = h[PS_JK("staticIp")].to<JsonObject>();
    writeIp(ip, PS_JK("ip"), hw.staticIp.ip);
    writeIp(ip, PS_JK("gateway"), hw.staticIp.gateway);
    writeIp(ip, PS_JK("subnet"), hw.staticIp.subnet);
    writeIp(ip, PS_JK("dns1"), hw.staticIp.dns1);
    writeIp(ip, PS_JK("dns2"), hw.staticIp.dns2);
  }
}

} // namespace

Result parse(const uint8_t* sector, size_t len, const Hardware& base, Block& out) {
  out = Block{};
  out.hardware = base;
  if (len < kHeaderSize)
    return Result::BadLength;
  bool erased = true;
  for (size_t i = 0; i < kHeaderSize && erased; i++)
    erased = sector[i] == 0xFF;
  if (erased)
    return Result::Empty;
  if (memcmp(sector, kMagic, sizeof(kMagic)) != 0)
    return Result::BadMagic;
  const uint32_t n = le32(sector + 8);
  if (n == 0 || n > len - kHeaderSize)
    return Result::BadLength;
  if (crc::crc32(sector + kHeaderSize, n) != le32(sector + 12))
    return Result::BadCrc;
  JsonDocument doc;
  if (deserializeJson(doc, reinterpret_cast<const char*>(sector + kHeaderSize), n))
    return Result::BadJson;
  JsonObjectConst h = doc[PS_JK("hardware")];
  if (!h.isNull()) {
    out.hasHardware = true;
    readHardware(h, out.hardware);
  }
  JsonObjectConst s = doc[PS_JK("settings")];
  Settings& st = out.settings;
  if (s[PS_JK("deviceName")].is<const char*>() && *s[PS_JK("deviceName")].as<const char*>()) {
    st.hasDeviceName = true;
    copyStr(s[PS_JK("deviceName")], st.deviceName, sizeof(st.deviceName));
  }
  if (s[PS_JK("wifiSsid")].is<const char*>() && *s[PS_JK("wifiSsid")].as<const char*>()) {
    st.hasWifi = true;
    copyStr(s[PS_JK("wifiSsid")], st.wifiSsid, sizeof(st.wifiSsid));
    copyStr(s[PS_JK("wifiPassword")], st.wifiPassword, sizeof(st.wifiPassword));
  }
  if (s[PS_JK("adminPassword")].is<const char*>() && *s[PS_JK("adminPassword")].as<const char*>()) {
    st.hasAdminPassword = true;
    copyStr(s[PS_JK("adminPassword")], st.adminPassword, sizeof(st.adminPassword));
  }
  return Result::Ok;
}

bool encode(const Block& b, bool withSettings, uint8_t* sector, size_t len) {
  if (len < kSectorSize)
    return false;
  JsonDocument doc;
  if (b.hasHardware)
    writeHardware(doc[PS_JK("hardware")].to<JsonObject>(), b.hardware);
  if (withSettings && b.settings.any()) {
    JsonObject s = doc[PS_JK("settings")].to<JsonObject>();
    if (b.settings.hasDeviceName)
      s[PS_JK("deviceName")] = b.settings.deviceName;
    if (b.settings.hasWifi) {
      s[PS_JK("wifiSsid")] = b.settings.wifiSsid;
      s[PS_JK("wifiPassword")] = b.settings.wifiPassword;
    }
    if (b.settings.hasAdminPassword)
      s[PS_JK("adminPassword")] = b.settings.adminPassword;
  }
  memset(sector, 0xFF, kSectorSize);
  const size_t n =
      serializeJson(doc, reinterpret_cast<char*>(sector + kHeaderSize), kSectorSize - kHeaderSize);
  if (n == 0 || n >= kSectorSize - kHeaderSize)
    return false;
  memset(sector + kHeaderSize + n, 0xFF, kSectorSize - kHeaderSize - n);
  memcpy(sector, kMagic, sizeof(kMagic));
  putLe32(sector + 8, static_cast<uint32_t>(n));
  putLe32(sector + 12, crc::crc32(sector + kHeaderSize, n));
  return true;
}

const char* resultName(Result r) {
  switch (r) {
  case Result::Ok:
    return "ok";
  case Result::Empty:
    return "leer";
  case Result::BadMagic:
    return "keine Kennung";
  case Result::BadLength:
    return "Länge falsch";
  case Result::BadCrc:
    return "Prüfsumme falsch";
  case Result::BadJson:
    return "JSON ungültig";
  }
  return "?";
}

} // namespace provision
} // namespace ps
