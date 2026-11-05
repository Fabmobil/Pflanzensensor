/**
 * @file router.cpp
 * @brief Routentabelle und Dispatch
 */
#include "web/router.h"

#include "hal/system.h"
#include "services/log.h"
#include "web/assets.h"
#include "web/respond.h"

namespace ps {
namespace web {

namespace {

const Route kRoutes[] PS_PROGMEM = {
    // Kompatibel (E8)
    {"/getLatestValues", MGet, false, Access::Public, getLatestValues},
    {"/metrics", MGet, false, Access::Public, metrics},
    {"/status", MGet, false, Access::Public, status},
    // Öffentlich
    {"/api/v1/system", MGet, false, Access::Public, system},
    {"/api/v1/measure", MPost, false, Access::Public, measurePublic},
    {"/api/v1/chronik/data", MGet, false, Access::Public, chronikData},
    {"/api/v1/chronik/info", MGet, false, Access::Public, chronikInfo},
    {"/api/v1/chronik/export.csv", MGet, false, Access::Public, chronikCsv},
    // Admin
    {"/api/v1/config/export", MGet, false, Access::Admin, configExport},
    {"/api/v1/config/import", MPost, false, Access::Admin, configImport},
    {"/api/v1/config", MGet | MPatch, false, Access::Admin, configGet},
    {"/api/v1/secrets/admin-password", MPut, false, Access::Admin, secretAdmin},
    {"/api/v1/secrets/wifi/", MPut, true, Access::Admin, secretWifi},
    {"/api/v1/secrets/mail-key", MPut, false, Access::Admin, secretMailKey},
    {"/api/v1/sensors", MGet | MPatch, false, Access::Admin, sensorsGet},
    {"/api/v1/sensors/", MPost, true, Access::Admin, sensorsAction},
    {"/api/v1/logs/file", MGet, false, Access::Public, logsFile},
    {"/api/v1/logs/settings", MGet | MPatch, false, Access::Public, logsSettings},
    {"/api/v1/login", MPost, false, Access::Public, login},
    {"/api/v1/logout", MPost, false, Access::Public, logout},
    {"/api/v1/logs", MGet, false, Access::Public, logsGet},
    {"/api/v1/mail/test", MPost, false, Access::Admin, mailTest},
    {"/api/v1/mail/status", MGet, false, Access::Admin, mailStatus},
    {"/api/v1/mail/templates/", MGet | MPut | MDelete, true, Access::Admin, mailTemplates},
    {"/api/v1/system/reboot", MPost, false, Access::Admin, reboot},
    {"/api/v1/system/factory-reset", MPost, false, Access::Admin, factoryReset},
    {"/api/v1/ota", MPost | MGet, false, Access::Admin, otaDone},
};

const Page kPages[] PS_PROGMEM = {
    {"/", "/index.html", Access::Public},
    {"/chronik", "/chronik.html", Access::Public},
    {"/logs", "/logs.html", Access::Public},
    {"/login", "/login.html", Access::Public},
    {"/admin", "/admin.html", Access::Admin},
    {"/admin/sensors", "/sensors.html", Access::Admin},
    {"/admin/display", "/display.html", Access::Admin},
    {"/admin/mail", "/mail.html", Access::Admin},
    {"/admin/mail/templates", "/mail-templates.html", Access::Admin},
    {"/admin/update", "/update.html", Access::Admin},
};

} // namespace

static const char* methodName(hal::HttpMethod m) {
  switch (m) {
  case hal::HttpMethod::Get:
    return "GET";
  case hal::HttpMethod::Post:
    return "POST";
  case hal::HttpMethod::Put:
    return "PUT";
  case hal::HttpMethod::Patch:
    return "PATCH";
  case hal::HttpMethod::Delete:
    return "DELETE";
  default:
    return "?";
  }
}

uint8_t methodBit(hal::HttpMethod m) {
  switch (m) {
  case hal::HttpMethod::Get:
    return MGet;
  case hal::HttpMethod::Post:
    return MPost;
  case hal::HttpMethod::Put:
    return MPut;
  case hal::HttpMethod::Patch:
    return MPatch;
  case hal::HttpMethod::Delete:
    return MDelete;
  default:
    return 0;
  }
}

void Router::dispatch(hal::Request& req, hal::Response& res, void* self) {
  static_cast<Router*>(self)->handle(req, res);
}

bool Router::authorized(const hal::Request& req) {
  return ctx_.auth.check(req, hal::now().millis()) == Auth::Result::Ok;
}

bool Router::requireAdmin(hal::Request& req, hal::Response& res, bool page) {
  const Auth::Result r = ctx_.auth.check(req, hal::now().millis());
  if (r == Auth::Result::Ok)
    return true;
  if (page) {
    // Seiten: zur Anmeldung und danach zurück
    char loc[96];
    snprintf(loc, sizeof(loc), "/login?next=%s", req.path());
    res.header("Location", loc);
    res.header("Cache-Control", "no-store");
    res.begin(302, "text/plain", 0);
    res.end();
    return false;
  }
  if (r == Auth::Result::Locked) {
    char s[8];
    snprintf(s, sizeof(s), "%lu",
             static_cast<unsigned long>(ctx_.auth.lockedFor(hal::now().millis()) / 1000 + 1));
    res.header("Retry-After", s);
    sendError(res, 429, Err::Throttled, "Zu viele Fehlversuche - bitte 30 s warten");
    return false;
  }
  // Kein WWW-Authenticate: der Browser soll keinen Benutzer/Passwort-Dialog
  // zeigen, die Seiten leiten selbst zu /login. Skripte schicken Basic-Auth
  // ohnehin gleich mit.
  sendError(res, 401, Err::Auth, "Bitte anmelden");
  return false;
}

bool Router::serveAsset(const char* path, hal::Request& req, hal::Response& res) {
  for (uint8_t i = 0; i < assets::kCount; i++) {
    assets::Asset a;
    ps_memcpy_P(&a, &assets::kAssets[i], sizeof(a));
    if (strcmp(a.path, path) != 0)
      continue;
    char inm[16];
    char etag[16];
    snprintf(etag, sizeof(etag), "\"%s\"", a.etag);
    res.header("ETag", etag);
    res.header("Cache-Control", a.html ? "no-cache" : "max-age=86400");
    if (req.header("If-None-Match", inm, sizeof(inm)) && strcmp(inm, etag) == 0) {
      res.begin(304, a.type, 0);
      res.end();
      return true;
    }
    res.header("Content-Encoding", "gzip");
    res.begin(200, a.type, static_cast<int32_t>(a.len));
    res.writeFlash(a.data, a.len);
    res.end();
    return true;
  }
  return false;
}

bool Router::captive(hal::Request& req, hal::Response& res) {
  // Im AP-Modus führen die Prüfadressen der Betriebssysteme und jeder andere
  // unbekannte Pfad zur Einrichtung (Captive-Portal, E9).
  if (!ctx_.net.apActive())
    return false;
  (void)req;
  char ip[16];
  ctx_.net.ip(ip, sizeof(ip));
  char loc[40];
  snprintf(loc, sizeof(loc), "http://%s/admin", ip);
  res.header("Location", loc);
  res.begin(302, "text/plain", 0);
  res.end();
  return true;
}

void Router::handle(hal::Request& req, hal::Response& res) {
  const char* path = req.path();
  const uint8_t mbit = methodBit(req.method());
  // Die Logs-Seite fragt jede Sekunde nach - das gehört nicht ins Log
  if (strncmp(path, "/api/v1/logs", 12) != 0)
    LOGD(Mod::Web, "%s %s", methodName(req.method()), path);

  // Seiten
  for (const auto& pp : kPages) {
    Page p;
    ps_memcpy_P(&p, &pp, sizeof(p));
    if (strcmp(p.path, path) != 0)
      continue;
    if (mbit != MGet)
      break;
    if (p.access == Access::Admin && !requireAdmin(req, res, true))
      return;
    if (!serveAsset(p.asset, req, res))
      sendError(res, 404, Err::NotFound);
    return;
  }

  // API
  bool pathKnown = false;
  for (const auto& rr : kRoutes) {
    Route r;
    ps_memcpy_P(&r, &rr, sizeof(r));
    const size_t plen = strlen(r.path);
    const bool hit = r.prefix ? strncmp(path, r.path, plen) == 0 && path[plen] != '\0'
                              : strcmp(path, r.path) == 0;
    if (!hit)
      continue;
    pathKnown = true;
    if (!(r.methods & mbit))
      continue;
    if (r.access == Access::Admin && !requireAdmin(req, res, false))
      return;
    r.fn(ctx_, req, res, r.prefix ? path + plen : "");
    return;
  }
  if (pathKnown) {
    sendError(res, 405, Err::Unsupported, "Methode nicht erlaubt");
    return;
  }

  // Statische Dateien (öffentlich)
  if (mbit == MGet) {
    if (strcmp(path, "/favicon.ico") == 0 || strncmp(path, "/css/", 5) == 0 ||
        strncmp(path, "/js/", 4) == 0 || strncmp(path, "/img/", 5) == 0) {
      if (serveAsset(path, req, res))
        return;
    }
  }
  if (mbit == MGet && captive(req, res))
    return;
  sendError(res, 404, Err::NotFound);
}

} // namespace web
} // namespace ps
