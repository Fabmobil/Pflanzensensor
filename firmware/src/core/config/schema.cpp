/**
 * @file schema.cpp
 * @brief Schematabellen und generisches Lesen/Schreiben
 */
#include "core/config/schema.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "core/sensors/status.h"

namespace ps {
namespace schema {

namespace {

using FT = FieldType;

#define DEV(sec, k, type, flags, member, mn, mx, en)                                               \
  {sec,                                                                                            \
   k,                                                                                              \
   type,                                                                                           \
   flags,                                                                                          \
   static_cast<uint16_t>(offsetof(DeviceConfig, member)),                                          \
   static_cast<uint16_t>(sizeof(DeviceConfig::member)),                                            \
   mn,                                                                                             \
   mx,                                                                                             \
   en}

const FieldDef kDevice[] PS_PROGMEM = {
    DEV("general", "deviceName", FT::Str, FNonEmpty, deviceName, 0, 0, ""),
    DEV("general", "otaRequireMd5", FT::Bool, FNone, otaRequireMd5, 0, 0, ""),
    DEV("general", "flowerChannel", FT::Str, FChannelKey, flowerChannel, 0, 0, ""),
    DEV("wifi", "ssid1", FT::Str, FNone, wifiSsid[0], 0, 0, ""),
    DEV("wifi", "ssid2", FT::Str, FNone, wifiSsid[1], 0, 0, ""),
    DEV("wifi", "ssid3", FT::Str, FNone, wifiSsid[2], 0, 0, ""),
    DEV("display", "screenDuration", FT::U8, FNone, screenDurationS, 1, 60, ""),
    DEV("display", "clockFormat", FT::Enum, FNone, clockFormat, 0, 1, "24h|12h"),
    DEV("display", "showIp", FT::Bool, FNone, showIp, 0, 0, ""),
    DEV("display", "showClock", FT::Bool, FNone, showClock, 0, 0, ""),
    DEV("display", "showQr", FT::Bool, FNone, showQr, 0, 0, ""),
    DEV("display", "showFlower", FT::Bool, FNone, showFlower, 0, 0, ""),
    DEV("display", "showFabmobil", FT::Bool, FNone, showFabmobil, 0, 0, ""),
    DEV("led", "mode", FT::U8, FNone, ledMode, 0, 2, ""),
    DEV("led", "channel", FT::Str, FChannelKey, ledChannel, 0, 0, ""),
    DEV("led", "onlyRed", FT::Bool, FNone, ledOnlyRed, 0, 0, ""),
    DEV("log", "level", FT::Enum, FNone, logLevel, 0, 3, "DEBUG|INFO|WARNING|ERROR"),
    DEV("log", "file", FT::Bool, FNone, fileLog, 0, 0, ""),
    DEV("log", "debugRam", FT::Bool, FNone, debugRam, 0, 0, ""),
    DEV("log", "debugCycle", FT::Bool, FNone, debugCycle, 0, 0, ""),
    DEV("log", "debugSensor", FT::Bool, FNone, debugSensor, 0, 0, ""),
    DEV("log", "debugDisplay", FT::Bool, FNone, debugDisplay, 0, 0, ""),
    DEV("log", "debugWeb", FT::Bool, FNone, debugWeb, 0, 0, ""),
    DEV("log", "debugMail", FT::Bool, FNone, debugMail, 0, 0, ""),
    DEV("mail", "enabled", FT::Bool, FNone, mailEnabled, 0, 0, ""),
    DEV("mail", "url", FT::Str, FUrl, mailUrl, 0, 0, ""),
    DEV("mail", "deviceId", FT::Str, FPrintable, mailDeviceId, 0, 0, ""),
    DEV("mail", "to", FT::Str, FNone, mailTo, 0, 0, ""),
    DEV("mail", "watch", FT::Str, FNone, mailWatch, 0, 0, ""),
    DEV("mail", "warnFrom", FT::Enum, FNone, mailWarnFrom, 0, 1, "yellow|red"),
    DEV("mail", "warnHours", FT::U16, FNone, mailWarnHours, 1, 720, ""),
    DEV("mail", "boot", FT::Bool, FNone, mailBoot, 0, 0, ""),
    DEV("mail", "alive", FT::Bool, FNone, mailAlive, 0, 0, ""),
    DEV("mail", "aliveHours", FT::U16, FNone, mailAliveHours, 1, 720, ""),
};
#undef DEV

#define SEN(k, type, flags, member, mn, mx)                                                        \
  {"",                                                                                             \
   k,                                                                                              \
   type,                                                                                           \
   flags,                                                                                          \
   static_cast<uint16_t>(offsetof(SensorConfig, member)),                                          \
   static_cast<uint16_t>(sizeof(SensorConfig::member)),                                            \
   mn,                                                                                             \
   mx,                                                                                             \
   ""}

const FieldDef kSensor[] PS_PROGMEM = {
    SEN("enabled", FT::Bool, FNone, enabled, 0, 0),
    SEN("interval", FT::U16, FNone, intervalS, 10, 3600),
    SEN("scale", FT::F32, FNone, scale, -1e9f, 1e9f),
    SEN("tare", FT::I32, FNone, tare, -2147483647.0f, 2147483647.0f),
};
#undef SEN

#define CH(k, type, flags, member, mn, mx)                                                         \
  {"",                                                                                             \
   k,                                                                                              \
   type,                                                                                           \
   flags,                                                                                          \
   static_cast<uint16_t>(offsetof(ChannelConfig, member)),                                         \
   static_cast<uint16_t>(sizeof(ChannelConfig::member)),                                           \
   mn,                                                                                             \
   mx,                                                                                             \
   ""}

const FieldDef kChannel[] PS_PROGMEM = {
    CH("enabled", FT::Bool, FNone, enabled, 0, 0),
    CH("display", FT::Bool, FNone, display, 0, 0),
    CH("name", FT::Str, FNonEmpty, name, 0, 0),
    CH("unit", FT::Str, FNone, unit, 0, 0),
    CH("field", FT::Str, FNone, field, 0, 0),
    CH("yellowLow", FT::F32, FNone, limits.yellowLow, -1e12f, 1e12f),
    CH("greenLow", FT::F32, FNone, limits.greenLow, -1e12f, 1e12f),
    CH("greenHigh", FT::F32, FNone, limits.greenHigh, -1e12f, 1e12f),
    CH("yellowHigh", FT::F32, FNone, limits.yellowHigh, -1e12f, 1e12f),
    CH("rawMin", FT::I16, FNone, rawMin, 0, 4095),
    CH("rawMax", FT::I16, FNone, rawMax, 0, 4095),
    CH("inverted", FT::Bool, FNone, inverted, 0, 0),
    CH("autocal", FT::Bool, FNone, autocal, 0, 0),
    CH("halfLife", FT::U32, FHalfLife, halfLifeS, 0, 2592000),
};
#undef CH

const Table kDeviceTable{kDevice, sizeof(kDevice) / sizeof(kDevice[0]), true};
const Table kSensorTable{kSensor, sizeof(kSensor) / sizeof(kSensor[0]), false};
const Table kChannelTable{kChannel, sizeof(kChannel) / sizeof(kChannel[0]), false};

/// Enum-Name an Position idx in "A|B|C" finden
bool enumName(const char* names, uint8_t idx, char* out, size_t cap) {
  uint8_t i = 0;
  const char* p = names;
  while (*p) {
    const char* e = strchr(p, '|');
    const size_t len = e ? static_cast<size_t>(e - p) : strlen(p);
    if (i == idx) {
      const size_t n = len < cap - 1 ? len : cap - 1;
      memcpy(out, p, n);
      out[n] = '\0';
      return true;
    }
    if (!e)
      break;
    p = e + 1;
    i++;
  }
  return false;
}

int enumIndex(const char* names, const char* value) {
  int i = 0;
  const char* p = names;
  const size_t vlen = strlen(value);
  while (*p) {
    const char* e = strchr(p, '|');
    const size_t len = e ? static_cast<size_t>(e - p) : strlen(p);
    if (len == vlen && strncasecmp(p, value, len) == 0)
      return i;
    if (!e)
      break;
    p = e + 1;
    i++;
  }
  return -1;
}

bool isInt(JsonVariantConst v) { return v.is<int32_t>() || v.is<uint32_t>() || v.is<int64_t>(); }

bool readNumber(JsonVariantConst v, double& out) {
  if (v.is<bool>())
    return false;
  // Auswahllisten im Browser liefern Zahlen als Text ("2")
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    char* end = nullptr;
    out = strtod(s, &end);
    return *s && end && *end == '\0' && !isnan(out) && !isinf(out);
  }
  if (isInt(v)) {
    out = v.as<double>();
    return true;
  }
  if (v.is<double>() || v.is<float>()) {
    out = v.as<double>();
    return !isnan(out) && !isinf(out);
  }
  return false;
}

bool urlOk(const char* s) {
  return strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0;
}

bool printable(const char* s) {
  for (; *s; s++)
    if (static_cast<uint8_t>(*s) < 0x20 || static_cast<uint8_t>(*s) > 0x7E)
      return false;
  return true;
}

bool hasControlChars(const char* s) {
  for (; *s; s++)
    if (static_cast<uint8_t>(*s) < 0x20)
      return true;
  return false;
}

} // namespace

const Table& deviceTable() { return kDeviceTable; }
const Table& sensorTable() { return kSensorTable; }
const Table& channelTable() { return kChannelTable; }

FieldDef field(const Table& t, uint8_t i) {
  FieldDef f{};
  if (i < t.count)
    ps_memcpy_P(&f, &t.fields[i], sizeof(f));
  return f;
}

void FieldErrors::add(const char* path, Err e) {
  if (count >= kMax) {
    dropped++;
    return;
  }
  items[count].path.assign(path);
  items[count].err = e;
  count++;
}

void FieldErrors::addParts(const char* a, const char* b, const char* c, Err e) {
  char buf[48];
  snprintf(buf, sizeof(buf), "%s%s%s%s%s", a, (a[0] && b[0]) ? "." : "", b, (c && c[0]) ? "." : "",
           c ? c : "");
  add(buf, e);
}

void FieldErrors::writeJson(JsonWriter& w) const {
  w.beginObject();
  for (uint8_t i = 0; i < count; i++) {
    w.key(items[i].path.c_str());
    w.value(errCode(items[i].err));
  }
  w.endObject();
}

void keyOfSlot(uint8_t slot, char* out, size_t cap) {
  if (slot >= board::kSlotCount) {
    if (cap)
      out[0] = '\0';
    return;
  }
  char id[16];
  copyP(id, sizeof(id), catalog::id(board::kSensors[board::kSlots[slot].sensor].type));
  snprintf(out, cap, "%s_%u", id, static_cast<unsigned>(board::kSlots[slot].channel));
}

int slotOfKey(const char* key) {
  if (!key)
    return -1;
  const char* us = strrchr(key, '_');
  if (!us || us == key || !us[1])
    return -1;
  char* end = nullptr;
  const long idx = strtol(us + 1, &end, 10);
  if (!end || *end != '\0' || idx < 0)
    return -1;
  const SensorType t = catalog::typeFromId(key, static_cast<size_t>(us - key));
  if (t == SensorType::Count)
    return -1;
  const int si = board::sensorIndexOf(t);
  if (si < 0)
    return -1;
  if (idx >= board::kSensors[si].channels)
    return -1;
  return board::kSensors[si].firstSlot + static_cast<int>(idx);
}

bool isChannelKey(const char* s) { return slotOfKey(s) >= 0; }

Err setField(const FieldDef& f, JsonVariantConst v, void* base) {
  uint8_t* p = static_cast<uint8_t*>(base) + f.offset;
  switch (f.type) {
  case FT::Bool: {
    if (!v.is<bool>())
      return Err::InvalidArgument;
    const bool b = v.as<bool>();
    memcpy(p, &b, sizeof(b));
    return Err::None;
  }
  case FT::Enum: {
    int idx = -1;
    if (v.is<const char*>()) {
      idx = enumIndex(f.enumNames, v.as<const char*>());
    } else if (isInt(v)) {
      idx = v.as<int>();
    }
    if (idx < static_cast<int>(f.min) || idx > static_cast<int>(f.max))
      return Err::OutOfRange;
    const uint8_t u = static_cast<uint8_t>(idx);
    memcpy(p, &u, 1);
    return Err::None;
  }
  case FT::Str: {
    if (!v.is<const char*>())
      return Err::InvalidArgument;
    const char* s = v.as<const char*>();
    const size_t len = strlen(s);
    if (len > static_cast<size_t>(f.size - 1))
      return Err::TooLong;
    if (hasControlChars(s))
      return Err::InvalidArgument;
    if ((f.flags & FNonEmpty) && len == 0)
      return Err::InvalidArgument;
    if ((f.flags & FChannelKey) && len > 0 && !isChannelKey(s))
      return Err::NotFound;
    if ((f.flags & FUrl) && len > 0 && !urlOk(s))
      return Err::InvalidArgument;
    if ((f.flags & FPrintable) && !printable(s))
      return Err::InvalidArgument;
    memcpy(p, s, len);
    p[len] = '\0';
    return Err::None;
  }
  default:
    break;
  }
  double d;
  if (!readNumber(v, d))
    return Err::InvalidArgument;
  if (f.type != FT::F32 && d != floor(d))
    return Err::InvalidArgument;
  if (f.min != 0 || f.max != 0) {
    if (d < f.min || d > f.max)
      return Err::OutOfRange;
  }
  switch (f.type) {
  case FT::U8: {
    if (d < 0 || d > 255)
      return Err::OutOfRange;
    const uint8_t u = static_cast<uint8_t>(d);
    memcpy(p, &u, 1);
    break;
  }
  case FT::U16: {
    if (d < 0 || d > 65535)
      return Err::OutOfRange;
    const uint16_t u = static_cast<uint16_t>(d);
    memcpy(p, &u, 2);
    break;
  }
  case FT::U32: {
    if (d < 0 || d > 4294967295.0)
      return Err::OutOfRange;
    const uint32_t u = static_cast<uint32_t>(d);
    if ((f.flags & FHalfLife) && !halfLifeAllowed(u))
      return Err::OutOfRange;
    memcpy(p, &u, 4);
    break;
  }
  case FT::I16: {
    if (d < -32768 || d > 32767)
      return Err::OutOfRange;
    const int16_t u = static_cast<int16_t>(d);
    memcpy(p, &u, 2);
    break;
  }
  case FT::I32: {
    if (d < -2147483648.0 || d > 2147483647.0)
      return Err::OutOfRange;
    const int32_t u = static_cast<int32_t>(d);
    memcpy(p, &u, 4);
    break;
  }
  case FT::F32: {
    const float u = static_cast<float>(d);
    memcpy(p, &u, 4);
    break;
  }
  default:
    return Err::Unsupported;
  }
  return Err::None;
}

void writeValue(JsonWriter& w, const FieldDef& f, const void* base) {
  const uint8_t* p = static_cast<const uint8_t*>(base) + f.offset;
  switch (f.type) {
  case FT::Bool: {
    bool b;
    memcpy(&b, p, sizeof(b));
    w.value(b);
    return;
  }
  case FT::Enum: {
    char name[16];
    if (enumName(f.enumNames, p[0], name, sizeof(name)))
      w.value(name);
    else
      w.value(static_cast<uint32_t>(p[0]));
    return;
  }
  case FT::Str:
    w.value(reinterpret_cast<const char*>(p), strnlen(reinterpret_cast<const char*>(p), f.size));
    return;
  case FT::U8:
    w.value(static_cast<uint32_t>(p[0]));
    return;
  case FT::U16: {
    uint16_t u;
    memcpy(&u, p, 2);
    w.value(static_cast<uint32_t>(u));
    return;
  }
  case FT::U32: {
    uint32_t u;
    memcpy(&u, p, 4);
    w.value(u);
    return;
  }
  case FT::I16: {
    int16_t u;
    memcpy(&u, p, 2);
    w.value(static_cast<int32_t>(u));
    return;
  }
  case FT::I32: {
    int32_t u;
    memcpy(&u, p, 4);
    w.value(u);
    return;
  }
  case FT::F32: {
    float u;
    memcpy(&u, p, 4);
    w.value(u, 3);
    return;
  }
  }
}

uint8_t apply(const Table& t, JsonObjectConst obj, void* base, FieldErrors& errs, bool strict,
              const char* prefix) {
  uint8_t applied = 0;
  if (t.sectioned) {
    for (JsonPairConst sec : obj) {
      const char* secName = sec.key().c_str();
      if (strcmp(secName, "schema") == 0)
        continue;
      JsonObjectConst so = sec.value().as<JsonObjectConst>();
      bool secKnown = false;
      for (uint8_t i = 0; i < t.count; i++) {
        const FieldDef f = field(t, i);
        if (strcmp(f.section, secName) == 0)
          secKnown = true;
      }
      if (!secKnown || so.isNull()) {
        if (strict)
          errs.addParts(prefix, secName, nullptr, Err::NotFound);
        continue;
      }
      for (JsonPairConst kv : so) {
        bool found = false;
        for (uint8_t i = 0; i < t.count; i++) {
          const FieldDef f = field(t, i);
          if (strcmp(f.section, secName) != 0 || strcmp(f.key, kv.key().c_str()) != 0)
            continue;
          found = true;
          const Err e = setField(f, kv.value(), base);
          if (e == Err::None)
            applied++;
          else
            errs.addParts(prefix, secName, f.key, e);
          break;
        }
        if (!found && strict)
          errs.addParts(prefix, secName, kv.key().c_str(), Err::NotFound);
      }
    }
    return applied;
  }
  for (JsonPairConst kv : obj) {
    bool found = false;
    for (uint8_t i = 0; i < t.count; i++) {
      const FieldDef f = field(t, i);
      if (strcmp(f.key, kv.key().c_str()) != 0)
        continue;
      found = true;
      const Err e = setField(f, kv.value(), base);
      if (e == Err::None)
        applied++;
      else
        errs.addParts(prefix, f.key, nullptr, e);
      break;
    }
    if (!found && strict)
      errs.addParts(prefix, kv.key().c_str(), nullptr, Err::NotFound);
  }
  return applied;
}

void writeAll(JsonWriter& w, const Table& t, const void* base) {
  if (!t.sectioned) {
    for (uint8_t i = 0; i < t.count; i++) {
      const FieldDef f = field(t, i);
      w.key(f.key);
      writeValue(w, f, base);
    }
    return;
  }
  char current[8] = "";
  bool open = false;
  for (uint8_t i = 0; i < t.count; i++) {
    const FieldDef f = field(t, i);
    if (strcmp(f.section, current) != 0) {
      if (open)
        w.endObject();
      memcpy(current, f.section, sizeof(current));
      w.key(f.section).beginObject();
      open = true;
    }
    w.key(f.key);
    writeValue(w, f, base);
  }
  if (open)
    w.endObject();
}

Err checkChannel(const ChannelConfig& c, bool oneSided) {
  if (!limitsOrdered(c.limits, oneSided))
    return Err::OutOfRange;
  if (c.rawMin > c.rawMax)
    return Err::OutOfRange;
  return Err::None;
}

} // namespace schema
} // namespace ps
