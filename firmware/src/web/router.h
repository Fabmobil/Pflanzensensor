/**
 * @file router.h
 * @brief Statische Routentabelle mit Zugriffsstufe je Route (refactor.md 5.9)
 * @details Keine Laufzeitregistrierung, kein Handler-Cache. Präfix-Routen
 *          (z.B. /api/v1/sensors/) bekommen den Rest des Pfads.
 */
#pragma once

#include "web/handlers.h"

namespace ps {
namespace web {

enum class Access : uint8_t { Public, Admin };

/// Methoden-Bitmaske
enum MethodBit : uint8_t {
  MGet = 1,
  MPost = 2,
  MPut = 4,
  MPatch = 8,
  MDelete = 16,
};

struct Route {
  char path[36];
  uint8_t methods;
  bool prefix;
  Access access;
  Handler fn;
};

/// Seite: Pfad → Asset
struct Page {
  char path[24];
  char asset[24];
  Access access;
};

class Router {
public:
  explicit Router(WebContext& ctx) : ctx_(ctx) {}
  /// Einstieg für den HTTP-Server
  static void dispatch(hal::Request& req, hal::Response& res, void* self);
  void handle(hal::Request& req, hal::Response& res);
  /// Für Upload-Routen: ist die Anfrage angemeldet?
  bool authorized(const hal::Request& req);

private:
  bool requireAdmin(hal::Request& req, hal::Response& res, bool page);
  bool serveAsset(const char* path, hal::Request& req, hal::Response& res);
  bool captive(hal::Request& req, hal::Response& res);

  WebContext& ctx_;
};

/// Methode als Bit
uint8_t methodBit(hal::HttpMethod m);

} // namespace web
} // namespace ps
