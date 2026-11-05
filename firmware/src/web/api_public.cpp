/**
 * @file api_public.cpp
 * @brief Öffentliche Endpunkte: /getLatestValues, /metrics, /status,
 *        /api/v1/system, /api/v1/measure, Chronik
 */
#include <stdio.h>

#include "core/util/format.h"
#include "core/util/throttle.h"
#include "core/version.h"
#include "hal/system.h"
#include "web/handlers.h"
#include "web/respond.h"

namespace ps {
namespace web {

namespace {
RequestThrottle g_csvThrottle(10000);
}

// ---------------------------------------------------------------------------
// GET /getLatestValues (kompatibel, refactor.md 3.1)
// ---------------------------------------------------------------------------
void getLatestValues(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  const Instant now = hal::now();
  beginJson(res);
  JsonWriter w(res);
  char ip[16];
  c.net.ip(ip, sizeof(ip));
  w.beginObject();
  w.kv(FS_("currentTime"), now.millis());
  w.kv(FS_("deviceName"), c.cfg.device().deviceName.c_str());
  w.kv(FS_("flowerStatusSensor"), c.cfg.device().flowerChannel.c_str());
  w.kv(FS_("ip"), static_cast<const char*>(ip));
  w.key(FS_("sensors")).beginObject();
  const SensorsConfig& sc = c.cfg.sensors();
  for (uint8_t s = 0; s < board::kSlotCount; s++) {
    const uint8_t sensor = board::kSlots[s].sensor;
    const ChannelConfig& cc = sc.channels[s];
    if (!sc.sensors[sensor].enabled || !cc.enabled)
      continue;
    const ChannelState& st = c.sensors.channel(s);
    // Kanäle ohne bisher erfolgreiche Messung fehlen (Altverhalten)
    if (!c.sensors.measuredOnce(sensor))
      continue;
    const SensorType type = board::kSensors[sensor].type;
    if (type == SensorType::Mhz19 && !(st.value > 0))
      continue; // ppm ≤ 0 übersprungen
    char key[32];
    schema::keyOfSlot(s, key, sizeof(key));
    w.key(key).beginObject();
    w.key(FS_("value")).value(st.value, 2);
    w.kv(FS_("unit"), cc.unit.c_str());
    w.kv(FS_("lastMeasurement"), c.sensors.lastMeasurement(sensor).millis());
    w.kv(FS_("measurementInterval"), static_cast<uint32_t>(sc.sensors[sensor].intervalS) * 1000UL);
    w.key(FS_("status")).value(statusName(c.sensors.status(s, now)));
    w.key(FS_("absoluteMin")).value(st.obsMin, 2);
    w.key(FS_("absoluteMax")).value(st.obsMax, 2);
    if (type == SensorType::Analog) {
      w.kv(FS_("raw"), static_cast<int32_t>(st.raw));
      if (st.hasRawObs()) {
        w.kv(FS_("absoluteRawMin"), static_cast<int32_t>(st.rawMin));
        w.kv(FS_("absoluteRawMax"), static_cast<int32_t>(st.rawMax));
      } else {
        w.key(FS_("absoluteRawMin")).null();
        w.key(FS_("absoluteRawMax")).null();
      }
      w.kv(FS_("calibrationMode"), cc.autocal);
      w.key(FS_("minmax")).beginObject();
      w.key(FS_("min")).value(static_cast<float>(cc.rawMin), 2);
      w.key(FS_("max")).value(static_cast<float>(cc.rawMax), 2);
      w.endObject();
    }
    w.endObject();
  }
  w.endObject();
  const hal::HeapInfo& h = c.sys.heap();
  w.key(FS_("system")).beginObject();
  w.kv(FS_("freeHeap"), hal::heap().free);
  w.kv(FS_("heapFragmentation"), static_cast<uint32_t>(h.fragmentation));
  w.kv(FS_("rebootCount"), c.sys.rebootCount());
  w.kv(FS_("version"), kVersion);
  w.kv(FS_("buildDate"), kBuildDate);
  w.kv(FS_("processedSensors"), static_cast<uint32_t>(c.sensors.processedSensors()));
  w.endObject();
  w.endObject();
  res.end();
}

// ---------------------------------------------------------------------------
void metrics(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  res.begin(200, "text/plain; version=0.0.4; charset=utf-8", -1);
  c.metrics.write(res);
  res.end();
}

void status(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  beginJson(res);
  JsonWriter w(res);
  w.beginObject();
  w.kv(FS_("uptime"), c.sys.uptimeS());
  w.kv(FS_("version"), kVersion);
  w.kv(FS_("bootId"), c.sys.bootId());
  w.kv(FS_("updating"), c.ota.active());
  w.endObject();
  res.end();
}

// ---------------------------------------------------------------------------
// GET /api/v1/system (Fußzeile, Startseite)
// ---------------------------------------------------------------------------
void system(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  beginJson(res);
  JsonWriter w(res);
  char ip[16], ssid[33], buf[32];
  c.net.ip(ip, sizeof(ip));
  c.net.ssid(ssid, sizeof(ssid));
  w.beginObject();
  w.kv(FS_("deviceName"), c.cfg.device().deviceName.c_str());
  w.kv(FS_("version"), kVersion);
  w.kv(FS_("buildDate"), kBuildDate);
  w.kv(FS_("platform"), board::kPlatformName);
  w.kv(FS_("board"), board::kName);
  w.kv(FS_("ip"), static_cast<const char*>(ip));
  w.kv(FS_("ssid"), static_cast<const char*>(ssid));
  w.kv(FS_("hostname"), c.net.hostname());
  w.kv(FS_("ap"), c.net.apActive());
  w.kv(FS_("rssi"), static_cast<int32_t>(c.net.rssi()));
  w.kv(FS_("uptime"), c.sys.uptimeS());
  w.kv(FS_("rebootCount"), c.sys.rebootCount());
  const uint32_t ep = hal::epoch();
  w.kv(FS_("timeValid"), epochValid(ep));
  hal::LocalTime lt;
  if (hal::toLocalTime(ep, lt)) {
    snprintf(buf, sizeof(buf), "%02u.%02u.%04u %02u:%02u", lt.day, lt.month, lt.year, lt.hour,
             lt.minute);
    w.kv(FS_("time"), static_cast<const char*>(buf));
  } else {
    w.key(FS_("time")).null();
  }
  w.kv(FS_("epoch"), ep);
  w.kv(FS_("flowerChannel"), c.cfg.device().flowerChannel.c_str());
  w.key(FS_("features")).beginObject();
  w.kv(FS_("display"), board::kFeatures.display);
  w.kv(FS_("led"), board::kFeatures.ledTraffic);
  w.kv(FS_("hx711"), board::kFeatures.hx711);
  w.endObject();
  w.key(FS_("heap")).beginObject();
  c.sys.writeHeap(w);
  w.endObject();
  // Aktive Kanäle für die Startseite (Name, Einheit, Sensor)
  w.key(FS_("channels")).beginArray();
  const SensorsConfig& sc = c.cfg.sensors();
  for (uint8_t s = 0; s < board::kSlotCount; s++) {
    const uint8_t sensor = board::kSlots[s].sensor;
    if (!sc.sensors[sensor].enabled || !sc.channels[s].enabled)
      continue;
    char key[32], id[16];
    schema::keyOfSlot(s, key, sizeof(key));
    copyP(id, sizeof(id), catalog::id(board::kSensors[sensor].type));
    w.beginObject();
    w.kv(FS_("key"), static_cast<const char*>(key));
    w.kv(FS_("sensor"), static_cast<const char*>(id));
    w.kv(FS_("name"), sc.channels[s].name.c_str());
    w.kv(FS_("unit"), sc.channels[s].unit.c_str());
    w.endObject();
  }
  w.endArray();
  w.endObject();
  res.end();
}

// ---------------------------------------------------------------------------
// POST /api/v1/measure {sensor} → 200/404/409/429
// ---------------------------------------------------------------------------
void measurePublic(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  char id[24] = "";
  JsonDocument doc;
  if (req.body() && req.bodyLength()) {
    if (!parseBody(req, res, doc))
      return;
    const char* s = doc["sensor"] | "";
    strncpy(id, s, sizeof(id) - 1);
  } else {
    req.arg("sensor", id, sizeof(id));
  }
  // "DHT_1" → Sensor "DHT" (Kanalschlüssel werden akzeptiert)
  int sensor = SensorService::sensorByName(id);
  if (sensor < 0) {
    const int slot = schema::slotOfKey(id);
    if (slot >= 0)
      sensor = board::kSlots[slot].sensor;
  }
  if (sensor < 0) {
    sendError(res, 404, Err::NotFound, "Sensor nicht gefunden");
    return;
  }
  uint32_t wait = 0;
  const MeasureResult r =
      c.sensors.measureNow(static_cast<uint8_t>(sensor), hal::now(), true, wait);
  char sec[8];
  switch (r) {
  case MeasureResult::Accepted:
    beginJson(res);
    res.write("{\"ok\":true,\"success\":true}");
    res.end();
    return;
  case MeasureResult::Throttled:
    snprintf(sec, sizeof(sec), "%lu", static_cast<unsigned long>(wait / 1000 + 1));
    res.header("Retry-After", sec);
    sendError(res, 429, Err::Throttled);
    return;
  case MeasureResult::Busy:
    sendError(res, 409, Err::Busy, "Messung läuft bereits");
    return;
  case MeasureResult::NotReady: {
    char msg[64];
    snprintf(msg, sizeof(msg), "Sensor wärmt noch %lu s auf",
             static_cast<unsigned long>(wait / 1000));
    snprintf(sec, sizeof(sec), "%lu", static_cast<unsigned long>(wait / 1000 + 1));
    res.header("Retry-After", sec);
    sendError(res, 409, Err::NotReady, msg);
    return;
  }
  case MeasureResult::Disabled:
    sendError(res, 409, Err::Disabled, "Sensor ist deaktiviert");
    return;
  default:
    sendError(res, 404, Err::NotFound);
  }
}

// ---------------------------------------------------------------------------
// Chronik
// ---------------------------------------------------------------------------
void chronikData(WebContext& c, hal::Request& req, hal::Response& res, const char*) {
  uint32_t since = 0;
  argU32(req, "since", since);
  res.header("Cache-Control", "no-store");
  res.begin(200, "application/octet-stream", -1);
  c.chronik.stream(since, res);
  res.end();
}

void chronikInfo(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  beginJson(res);
  JsonWriter w(res);
  c.chronik.writeInfo(w);
  res.end();
}

void chronikCsv(WebContext& c, hal::Request&, hal::Response& res, const char*) {
  if (!g_csvThrottle.tryAcquire(hal::now().millis())) {
    res.header("Retry-After", "10");
    sendError(res, 429, Err::Throttled, "Bitte kurz warten - der Export läuft schon");
    return;
  }
  res.header("Content-Disposition", "attachment; filename=chronik.csv");
  res.begin(200, "text/csv; charset=utf-8", -1);
  if (!c.chronik.exportCsv(res))
    res.write("# Abbruch: zu wenig Speicher\n");
  res.end();
}

} // namespace web
} // namespace ps
