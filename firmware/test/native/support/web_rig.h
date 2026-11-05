/**
 * @file web_rig.h
 * @brief Alle Dienste wie im Composition Root, auf Fakes, plus Router
 */
#pragma once

#include "fakes.h"
#include "web/router.h"
#include "web/uploads.h"

namespace pstest {

struct Rig {
  ps::native::MemFs fs;
  ps::TemplateStore tpl{fs};
  ps::StateStore st{fs};
  ps::ConfigService cfg{fs, tpl, st};
  ps::Events ev;
  ps::SensorService sensors{cfg, st, ev};
  ps::ChronikService chronik{fs, cfg, sensors};
  ps::LogService log{fs};
  ps::native::FakeWifi wifi;
  ps::NetService net{wifi, cfg, ev};
  ps::SystemInfo sys{fs};
  ps::native::FakeTcp tcp;
  ps::MailService mail{tcp, cfg, st, tpl, sensors, net, sys};
  ps::MetricsService metrics{cfg, sensors, net, sys};
  FakeOta otaPort;
  ps::OtaService ota{otaPort, cfg, ev};
  ps::Auth auth{cfg};
  ps::WebContext ctx{cfg, sensors, st,      chronik, log,  mail, tpl,
                     net, sys,     metrics, ota,     auth, ev,   fs};
  ps::web::Router router{ctx};
  ps::web::OtaUpload otaUpload{ctx, router};
  ps::web::ImportUpload importUpload{ctx, router};
  DriverSet drivers;

  Rig() {
    cfg.load();
    sys.begin();
  }
  /// Sensoren starten und messen lassen
  void measureAll(uint32_t ms = 20000) {
    sensors.begin(drivers.ptrs, ps::hal::now());
    for (uint32_t t = 0; t < ms; t += 100) {
      ps::native::advance(100);
      sensors.tick(ps::hal::now());
    }
  }
  FakeRes call(ps::hal::HttpMethod m, const char* path, const char* body = "",
               const char* pw = nullptr, std::map<std::string, std::string> args = {},
               std::map<std::string, std::string> headers = {}) {
    FakeReq q;
    q.m = m;
    q.p = path;
    q.b = body;
    q.args = args;
    q.headers = headers;
    if (pw)
      q.auth("admin", pw);
    FakeRes r;
    router.handle(q, r);
    return r;
  }
  FakeRes get(const char* path, const char* pw = nullptr) {
    return call(ps::hal::HttpMethod::Get, path, "", pw);
  }
  FakeRes admin(ps::hal::HttpMethod m, const char* path, const char* body = "") {
    return call(m, path, body, "Fabmobil");
  }
  /// Ereignistyp in der Warteschlange?
  bool popEvent(ps::EventType t, ps::Event* out = nullptr) {
    ps::Event e;
    bool found = false;
    while (ev.pop(e))
      if (e.type == t) {
        found = true;
        if (out)
          *out = e;
      }
    return found;
  }
};

} // namespace pstest
