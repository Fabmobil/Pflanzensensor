/**
 * @file legacy_import.cpp
 * @brief Zuordnung Altformat → Neu (Tabelle in refactor.md 2.3)
 */
#include "core/config/legacy_import.h"

#include <math.h>
#include <stdio.h>
#include <strings.h>

namespace ps {
namespace legacy {

namespace {

/// Text übernehmen, wenn vorhanden und passend
template <size_t N>
void takeStr(JsonVariantConst v, FixedString<N>& out, const char* path, Report& r,
             bool nonEmpty = false, bool channelKey = false) {
  if (v.isNull())
    return;
  if (!v.is<const char*>()) {
    r.bad(path);
    return;
  }
  const char* s = v.as<const char*>();
  if (strlen(s) > N - 1 || (nonEmpty && !*s) || (channelKey && *s && !schema::isChannelKey(s))) {
    r.bad(path);
    return;
  }
  out.assign(s);
  r.applied++;
}

void takeBool(JsonVariantConst v, bool& out, const char* path, Report& r) {
  if (v.isNull())
    return;
  if (v.is<bool>()) {
    out = v.as<bool>();
    r.applied++;
  } else if (v.is<int>()) {
    out = v.as<int>() != 0;
    r.applied++;
  } else {
    r.bad(path);
  }
}

bool number(JsonVariantConst v, double& d) {
  if (v.isNull() || v.is<bool>() || v.is<const char*>())
    return false;
  d = v.as<double>();
  return !isnan(d) && !isinf(d);
}

/// "ANALOG:1,0,1;DHT:1,1" → Display-Schalter je Kanal
void parseSensorDisp(const char* s, SensorsConfig& sens, Report& r) {
  const char* p = s;
  while (*p) {
    const char* end = strchr(p, ';');
    const size_t len = end ? static_cast<size_t>(end - p) : strlen(p);
    const char* colon = static_cast<const char*>(memchr(p, ':', len));
    if (colon) {
      const SensorType t = catalog::typeFromId(p, static_cast<size_t>(colon - p));
      const int si = t == SensorType::Count ? -1 : board::sensorIndexOf(t);
      if (si < 0) {
        r.ignored++;
      } else {
        uint8_t ch = 0;
        for (const char* q = colon + 1; q < p + len; q++) {
          if (*q == ',') {
            ch++;
            continue;
          }
          if (ch < board::kSensors[si].channels && (*q == '0' || *q == '1')) {
            sens.channels[board::kSensors[si].firstSlot + ch].display = (*q == '1');
            r.applied++;
          }
        }
      }
    }
    if (!end)
      break;
    p = end + 1;
  }
}

/// Eine Altmessung auf einen Kanal anwenden
void importMeasurement(JsonObjectConst m, uint8_t sensor, SensorType type, SensorsConfig& sens,
                       Extras& ex, Report& r, const char* id) {
  const int idx = m["idx"] | 0;
  char path[40];
  snprintf(path, sizeof(path), "sensors.%s.%d", id, idx);
  if (idx < 0 || idx >= board::kSensors[sensor].channels) {
    r.ignore(path);
    return;
  }
  const uint8_t slot = static_cast<uint8_t>(board::kSensors[sensor].firstSlot + idx);
  ChannelConfig cc = sens.channels[slot];
  const bool analog = type == SensorType::Analog;

  takeBool(m["en"], cc.enabled, path, r);
  takeStr(m["nm"], cc.name, path, r, true);
  takeStr(m["fn"], cc.field, path, r);
  takeStr(m["un"], cc.unit, path, r);
  JsonObjectConst th = m["thresh"].as<JsonObjectConst>();
  if (!th.isNull()) {
    Limits l = cc.limits;
    double d;
    if (number(th["yl"], d))
      l.yellowLow = static_cast<float>(d);
    if (number(th["gl"], d))
      l.greenLow = static_cast<float>(d);
    if (number(th["gh"], d))
      l.greenHigh = static_cast<float>(d);
    if (number(th["yh"], d))
      l.yellowHigh = static_cast<float>(d);
    if (limitsOrdered(l, doc::slotOneSided(slot))) {
      cc.limits = l;
      r.applied++;
    } else {
      r.bad(path);
    }
  }

  ChannelState& st = ex.state[slot];
  double d;
  if (analog) {
    if (number(m["min"], d) && d >= 0 && d <= 4095)
      cc.rawMin = static_cast<int16_t>(d);
    if (number(m["max"], d) && d >= 0 && d <= 4095)
      cc.rawMax = static_cast<int16_t>(d);
    if (cc.rawMin > cc.rawMax) {
      const int16_t t = cc.rawMin;
      cc.rawMin = cc.rawMax;
      cc.rawMax = t;
    }
    takeBool(m["inv"], cc.inverted, path, r);
    takeBool(m["cal"], cc.autocal, path, r);
    if (number(m["ahl"], d)) {
      if (halfLifeAllowed(static_cast<uint32_t>(d)))
        cc.halfLifeS = static_cast<uint32_t>(d);
      else
        r.bad(path);
    }
    if (number(m["rmin"], d) && d >= 0 && d <= 4095)
      st.rawMin = static_cast<int16_t>(d);
    if (number(m["rmax"], d) && d >= 0 && d <= 4095)
      st.rawMax = static_cast<int16_t>(d);
    // Altcode-Standard 0/1023 heißt "nie beobachtet"
    if (st.rawMin == 0 && st.rawMax == 1023)
      st.resetRawObs();
    if (number(m["amin"], d))
      st.obsMin = static_cast<float>(d);
    if (number(m["amax"], d))
      st.obsMax = static_cast<float>(d);
    if (number(m["lastRawValue"], d) && d >= 0 && d <= 4095)
      st.raw = static_cast<int16_t>(d);
    if (cc.autocal)
      st.cal = autocalStart(cc.rawMin, cc.rawMax, st.raw);
  }
  if (number(m["lastValue"], d))
    st.value = static_cast<float>(d);
  ex.hasState[slot] = true;
  sens.channels[slot] = cc;
}

} // namespace

bool isLegacy(JsonObjectConst root) {
  if (root.isNull() || !root["schema"].isNull())
    return false;
  return !root["general"].isNull() || !root["sensors"].isNull();
}

void import(JsonObjectConst root, DeviceConfig& dev, SensorsConfig& sens, Secrets& sec, Extras& ex,
            Report& r) {
  // general
  JsonObjectConst g = root["general"].as<JsonObjectConst>();
  if (!g.isNull()) {
    takeStr(g["device_name"], dev.deviceName, "general.device_name", r, true);
    if (g["admin_pwd"].is<const char*>()) {
      const char* pw = g["admin_pwd"].as<const char*>();
      bool ok = *pw && strlen(pw) <= 32;
      for (const char* q = pw; *q; q++)
        if (static_cast<uint8_t>(*q) < 0x20 || static_cast<uint8_t>(*q) > 0x7E)
          ok = false;
      if (ok) {
        ex.hasAdminPassword = true;
        ex.adminPassword.assign(pw);
        r.applied++;
      } else {
        r.bad("general.admin_pwd");
      }
    }
    takeBool(g["md5_verify"], dev.otaRequireMd5, "general.md5_verify", r);
    takeStr(g["flower_sens"], dev.flowerChannel, "general.flower_sens", r, false, true);
  }
  // Datei-Log: general.file_log hat Vorrang vor log.file_enabled
  JsonObjectConst lg = root["log"].as<JsonObjectConst>();
  if (!g.isNull() && !g["file_log"].isNull())
    takeBool(g["file_log"], dev.fileLog, "general.file_log", r);
  else if (!lg.isNull())
    takeBool(lg["file_enabled"], dev.fileLog, "log.file_enabled", r);
  if (!lg.isNull() && lg["level"].is<const char*>()) {
    const char* lv = lg["level"].as<const char*>();
    const char* names[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    bool ok = false;
    for (uint8_t i = 0; i < 4; i++)
      if (strcasecmp(lv, names[i]) == 0) {
        dev.logLevel = i;
        ok = true;
      }
    if (strcasecmp(lv, "WARN") == 0) {
      dev.logLevel = 2;
      ok = true;
    }
    if (ok)
      r.applied++;
    else
      r.bad("log.level");
  }

  // wifi
  JsonObjectConst w = root["wifi"].as<JsonObjectConst>();
  if (!w.isNull()) {
    const char* ssidKeys[] = {"ssid1", "ssid2", "ssid3"};
    const char* pwdKeys[] = {"pwd1", "pwd2", "pwd3"};
    for (uint8_t i = 0; i < 3; i++) {
      char path[16];
      snprintf(path, sizeof(path), "wifi.%s", ssidKeys[i]);
      takeStr(w[ssidKeys[i]], dev.wifiSsid[i], path, r);
      snprintf(path, sizeof(path), "wifi.%s", pwdKeys[i]);
      takeStr(w[pwdKeys[i]], sec.wifiPassword[i], path, r);
    }
  }

  // display
  JsonObjectConst ds = root["display"].as<JsonObjectConst>();
  if (!ds.isNull()) {
    takeBool(ds["show_ip"], dev.showIp, "display.show_ip", r);
    takeBool(ds["show_clock"], dev.showClock, "display.show_clock", r);
    takeBool(ds["show_flower"], dev.showFlower, "display.show_flower", r);
    takeBool(ds["show_fabmobil"], dev.showFabmobil, "display.show_fabmobil", r);
    takeBool(ds["show_qr"], dev.showQr, "display.show_qr", r);
    double d;
    if (number(ds["screen_dur"], d)) {
      // Altwert in ms; Werte < 1000 als Sekunden deuten (Befund B-04)
      double s = d < 1000 ? d : d / 1000.0;
      if (s >= 1 && s <= 60) {
        dev.screenDurationS = static_cast<uint8_t>(s);
        r.applied++;
      } else {
        r.bad("display.screen_dur");
      }
    }
    if (ds["clock_fmt"].is<const char*>()) {
      const char* f = ds["clock_fmt"].as<const char*>();
      if (strcmp(f, "24h") == 0 || strcmp(f, "12h") == 0) {
        dev.clockFormat = strcmp(f, "12h") == 0 ? 1 : 0;
        r.applied++;
      } else {
        r.bad("display.clock_fmt");
      }
    }
    if (ds["sensor_disp"].is<const char*>())
      parseSensorDisp(ds["sensor_disp"].as<const char*>(), sens, r);
  }

  // debug (debug.mail fehlt im Altexport → bleibt aus)
  JsonObjectConst db = root["debug"].as<JsonObjectConst>();
  if (!db.isNull()) {
    takeBool(db["ram"], dev.debugRam, "debug.ram", r);
    takeBool(db["meas_cycle"], dev.debugCycle, "debug.meas_cycle", r);
    takeBool(db["sensor"], dev.debugSensor, "debug.sensor", r);
    takeBool(db["display"], dev.debugDisplay, "debug.display", r);
    takeBool(db["websocket"], dev.debugWeb, "debug.websocket", r);
  }

  // LED-Ampel
  JsonObjectConst led = root["led_traffic"].as<JsonObjectConst>();
  if (!led.isNull()) {
    double d;
    if (number(led["mode"], d)) {
      if (d >= 0 && d <= 2 && !(d == 1 && !board::kFeatures.display)) {
        dev.ledMode = static_cast<uint8_t>(d);
        r.applied++;
      } else {
        r.bad("led_traffic.mode");
      }
    }
    takeStr(led["sel_meas"], dev.ledChannel, "led_traffic.sel_meas", r, false, true);
    takeBool(led["only_red"], dev.ledOnlyRed, "led_traffic.only_red", r);
  }

  // Sensoren
  JsonArrayConst sensors = root["sensors"].as<JsonArrayConst>();
  for (JsonObjectConst grp : sensors) {
    const char* id = grp["id"] | "";
    const SensorType t = catalog::typeFromId(id, strlen(id));
    const int si = t == SensorType::Count ? -1 : board::sensorIndexOf(t);
    if (si < 0) {
      char path[40];
      snprintf(path, sizeof(path), "sensors.%s", id);
      r.ignore(path);
      continue;
    }
    double d;
    if (number(grp["measInt"], d)) {
      double s = d / 1000.0; // ms
      if (s < 10)
        s = 10;
      if (s > 3600)
        s = 3600;
      sens.sensors[si].intervalS = static_cast<uint16_t>(s);
      r.applied++;
    }
    JsonArrayConst ms = grp["measurements"].as<JsonArrayConst>();
    if (!ms.isNull()) {
      for (JsonObjectConst m : ms)
        importMeasurement(m, static_cast<uint8_t>(si), t, sens, ex, r, id);
    } else {
      // Altstruktur: Gruppe = Messung, Felder direkt
      importMeasurement(grp, static_cast<uint8_t>(si), t, sens, ex, r, id);
    }
  }
}

} // namespace legacy
} // namespace ps
