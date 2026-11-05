/**
 * @file document.cpp
 */
#include "core/config/document.h"

#include <stdio.h>
#include <stdlib.h>

namespace ps {
namespace doc {

namespace {

void sensorId(uint8_t sensor, char* out, size_t cap) {
  copyP(out, cap, catalog::id(board::kSensors[sensor].type));
}

int sensorFromKey(const char* key) {
  const SensorType t = catalog::typeFromId(key, strlen(key));
  if (t == SensorType::Count)
    return -1;
  return board::sensorIndexOf(t);
}

} // namespace

bool slotOneSided(uint8_t slot) {
  if (slot >= board::kSlotCount)
    return false;
  const auto& s = board::kSlots[slot];
  return catalog::channel(board::kSensors[s.sensor].type, s.channel).oneSided;
}

bool slotHasRaw(uint8_t slot) {
  if (slot >= board::kSlotCount)
    return false;
  const auto& s = board::kSlots[slot];
  return catalog::channel(board::kSensors[s.sensor].type, s.channel).hasRaw;
}

void writeDevice(JsonWriter& w, const DeviceConfig& c) {
  w.beginObject();
  w.kv(FS_("schema"), kSchemaVersion);
  schema::writeAll(w, schema::deviceTable(), &c);
  w.endObject();
}

void readDevice(JsonObjectConst root, DeviceConfig& c, schema::FieldErrors& warnings) {
  if (root.isNull())
    return;
  schema::apply(schema::deviceTable(), root, &c, warnings, false);
}

void writeSensor(JsonWriter& w, const SensorsConfig& c, uint8_t sensor) {
  w.beginObject();
  schema::writeAll(w, schema::sensorTable(), &c.sensors[sensor]);
  w.key(FS_("channels")).beginObject();
  const auto& se = board::kSensors[sensor];
  for (uint8_t ch = 0; ch < se.channels; ch++) {
    char idx[4];
    snprintf(idx, sizeof(idx), "%u", static_cast<unsigned>(ch));
    w.key(idx).beginObject();
    schema::writeAll(w, schema::channelTable(), &c.channels[se.firstSlot + ch]);
    w.endObject();
  }
  w.endObject();
  w.endObject();
}

void writeSensors(JsonWriter& w, const SensorsConfig& c) {
  w.beginObject();
  for (uint8_t i = 0; i < board::kSensorCount; i++) {
    char id[16];
    sensorId(i, id, sizeof(id));
    w.key(id);
    writeSensor(w, c, i);
  }
  w.endObject();
}

uint16_t applySensors(JsonObjectConst sensors, SensorsConfig& c, schema::FieldErrors& errs,
                      bool strict, uint32_t* changedSensors) {
  uint16_t applied = 0;
  for (JsonPairConst sp : sensors) {
    const char* id = sp.key().c_str();
    const int si = sensorFromKey(id);
    JsonObjectConst so = sp.value().as<JsonObjectConst>();
    if (si < 0 || so.isNull()) {
      errs.add(id, Err::NotFound);
      continue;
    }
    // Sensorfelder (ohne "channels")
    SensorConfig sc = c.sensors[si];
    for (JsonPairConst kv : so) {
      if (strcmp(kv.key().c_str(), "channels") == 0)
        continue;
      bool found = false;
      for (uint8_t i = 0; i < schema::sensorTable().count; i++) {
        const schema::FieldDef f = schema::field(schema::sensorTable(), i);
        if (strcmp(f.key, kv.key().c_str()) != 0)
          continue;
        found = true;
        const Err e = schema::setField(f, kv.value(), &sc);
        if (e == Err::None)
          applied++;
        else
          errs.addParts(id, f.key, nullptr, e);
      }
      if (!found && strict)
        errs.addParts(id, kv.key().c_str(), nullptr, Err::NotFound);
    }
    if (memcmp(&sc, &c.sensors[si], sizeof(sc)) != 0 && changedSensors)
      *changedSensors |= (1u << si);
    c.sensors[si] = sc;

    JsonObjectConst chs = so["channels"].as<JsonObjectConst>();
    if (chs.isNull())
      continue;
    const auto& se = board::kSensors[si];
    for (JsonPairConst cp : chs) {
      char* end = nullptr;
      const long idx = strtol(cp.key().c_str(), &end, 10);
      if (!end || *end || idx < 0 || idx >= se.channels) {
        errs.addParts(id, "channels", cp.key().c_str(), Err::NotFound);
        continue;
      }
      const uint8_t slot = static_cast<uint8_t>(se.firstSlot + idx);
      ChannelConfig cc = c.channels[slot];
      char prefix[32];
      snprintf(prefix, sizeof(prefix), "%s.channels.%ld", id, idx);
      const uint8_t n = schema::apply(schema::channelTable(), cp.value().as<JsonObjectConst>(), &cc,
                                      errs, strict, prefix);
      const Err chk = schema::checkChannel(cc, slotOneSided(slot));
      if (chk != Err::None) {
        errs.addParts(prefix, "limits", nullptr, chk);
        continue; // ganzen Kanal verwerfen, Ordnung muss gelten
      }
      if (n > 0 && changedSensors)
        *changedSensors |= (1u << si);
      applied = static_cast<uint16_t>(applied + n);
      c.channels[slot] = cc;
    }
  }
  return applied;
}

void readSensors(JsonObjectConst root, SensorsConfig& c, schema::FieldErrors& warnings) {
  if (root.isNull())
    return;
  applySensors(root["sensors"].as<JsonObjectConst>(), c, warnings, false);
}

void toHex(const uint8_t* in, size_t n, char* out) {
  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    out[2 * i] = kHex[in[i] >> 4];
    out[2 * i + 1] = kHex[in[i] & 0xF];
  }
  out[2 * n] = '\0';
}

bool fromHex(const char* in, uint8_t* out, size_t n) {
  if (!in || strlen(in) != 2 * n)
    return false;
  for (size_t i = 0; i < n; i++) {
    uint8_t v = 0;
    for (int k = 0; k < 2; k++) {
      const char c = in[2 * i + k];
      v = static_cast<uint8_t>(v << 4);
      if (c >= '0' && c <= '9')
        v |= static_cast<uint8_t>(c - '0');
      else if (c >= 'a' && c <= 'f')
        v |= static_cast<uint8_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        v |= static_cast<uint8_t>(c - 'A' + 10);
      else
        return false;
    }
    out[i] = v;
  }
  return true;
}

void writeSecrets(JsonWriter& w, const Secrets& s) {
  w.beginObject();
  w.kv(FS_("schema"), kSchemaVersion);
  w.key(FS_("wifi")).beginArray();
  for (const auto& p : s.wifiPassword)
    w.value(p.c_str());
  w.endArray();
  if (s.adminSet) {
    char hex[65];
    w.key(FS_("admin")).beginObject();
    toHex(s.adminSalt, sizeof(s.adminSalt), hex);
    w.kv(FS_("salt"), static_cast<const char*>(hex));
    toHex(s.adminHash, sizeof(s.adminHash), hex);
    w.kv(FS_("hash"), static_cast<const char*>(hex));
    w.endObject();
  }
  w.kv(FS_("mailKey"), s.mailKey.c_str());
  w.endObject();
}

void readSecrets(JsonObjectConst root, Secrets& s) {
  if (root.isNull())
    return;
  JsonArrayConst wifi = root["wifi"].as<JsonArrayConst>();
  uint8_t i = 0;
  for (JsonVariantConst v : wifi) {
    if (i >= 3)
      break;
    if (v.is<const char*>())
      s.wifiPassword[i].assign(v.as<const char*>());
    i++;
  }
  JsonObjectConst admin = root["admin"].as<JsonObjectConst>();
  if (!admin.isNull()) {
    Secrets tmp = s;
    if (fromHex(admin["salt"] | "", tmp.adminSalt, sizeof(tmp.adminSalt)) &&
        fromHex(admin["hash"] | "", tmp.adminHash, sizeof(tmp.adminHash))) {
      tmp.adminSet = true;
      s = tmp;
    }
  }
  if (root["mailKey"].is<const char*>())
    s.mailKey.assign(root["mailKey"].as<const char*>());
}

} // namespace doc
} // namespace ps
