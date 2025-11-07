/**
 * @file web_router.cpp
 * @brief Implementation of WebRouter class
 */

#include "web/core/web_router.h"

#include <LittleFS.h>

#include "logger/logger.h"

WebRouter::WebRouter(ESP8266WebServer& server) : _server(server) {
  if (!hasEnoughMemory()) {
    logger.error(F("WebRouter"), F("Nicht genügend Speicher für WebRouter-Initialisierung"));
    return;
  }

  try {
    _routes.reserve(MAX_ROUTES);
    _middleware.reserve(MAX_MIDDLEWARE);

    logger.debug(F("WebRouter"), F("WebRouter mit Grenzen initialisiert:"));
    logger.debug(F("WebRouter"), String(F("- Max Routen: ")) + String(MAX_ROUTES));
    logger.debug(F("WebRouter"), String(F("- Max Middleware: ")) + String(MAX_MIDDLEWARE));
  } catch (const std::exception& e) {
    logger.error(F("WebRouter"), F("Zuweisung der Router-Puffer fehlgeschlagen"));
  }
}

RouterResult WebRouter::addRoute(HTTPMethod method, const String& url, HandlerCallback handler,
                                 const String& handlerType) {
  if (!hasEnoughMemory()) {
    logger.error(F("WebRouter"), F("Nicht genügend Speicher für Route: ") + url);
    return RouterResult::fail(RouterError::RESOURCE_ERROR, F("Nicht genügend Speicher"));
  }

  if (url.isEmpty() || !handler) {
    logger.error(F("WebRouter"), F("Ungültige Routen-Parameter für: ") + url);
    return RouterResult::fail(RouterError::INVALID_ROUTE, F("Ungültige Routen-Parameter"));
  }

  if (exceedsRouteLimit()) {
    logger.error(F("WebRouter"), F("Routen-Limit überschritten für: ") + url);
    return RouterResult::fail(RouterError::REGISTRATION_FAILED, F("Routen-Limit überschritten"));
  }

  // Check for duplicate before modifying anything
  for (const auto& route : _routes) {
    if (route.url == url && route.method == method) {
      // Route already exists - update handler if different handlerType
      logger.debug(F("WebRouter"),
                   F("Route bereits registriert: ") + methodToString(method) + F(" ") + url);
      return RouterResult::success();
    }
  }

  // Use provided handlerType, or fall back to context if not specified
  String effectiveHandlerType = handlerType.isEmpty() ? _currentHandlerType : handlerType;

  // Store route with handler type for cleanup tracking
  _routes.emplace_back(url, method, handler, effectiveHandlerType);

  // NOTE: We do NOT register with _server.on() because ESP8266WebServer
  // has no way to unregister routes. All routing goes through handleRequest()
  // which is called from onNotFound handler in setupRoutes().

  logRouteRegistration(method, url);
  return RouterResult::success();
}

void WebRouter::serveStatic(const String& urlPrefix, fs::FS& fs, const String& path, bool cache) {
  logger.debug(F("WebRouter"),
               String(F("Einrichte statische Route: ")) + urlPrefix + " -> " + path);

  if (!fs.exists(path)) {
    logger.warning(F("WebRouter"), String(F("Statische Datei nicht gefunden: ")) + path);
  }

  // Use ESP8266WebServer's built-in static file serving
  _server.serveStatic(urlPrefix.c_str(), fs, path.c_str(), cache ? "max-age=3600" : nullptr);

  logger.debug(F("WebRouter"),
               String(F("Statische Route registriert: ")) + urlPrefix + " -> " + path);
}

bool WebRouter::handleRequest(HTTPMethod method, const String& url) {
  if (!hasEnoughMemory()) {
    logger.error(F("WebRouter"), F("Wenig Speicher - Anfrage kann nicht verarbeitet werden"));
    return false;
  }

  if (!executeMiddleware(method, url)) {
    return false;
  }

  Route* route = findRoute(method, url);
  if (!route) {
    return false;
  }

  try {
    route->handler();
    return true;
  } catch (const std::exception& e) {
    logger.error(F("WebRouter"), F("Handler-Fehler: ") + String(e.what()));
    return false;
  }
}

void WebRouter::addMiddleware(MiddlewareCallback middleware) {
  if (!middleware || !hasEnoughMemory()) {
    logger.error(F("WebRouter"), F("Ungültige Middleware oder wenig Speicher"));
    return;
  }

  if (exceedsMiddlewareLimit()) {
    logger.error(F("WebRouter"), F("Middleware-Limit erreicht"));
    return;
  }

  try {
    _middleware.emplace_back(std::move(middleware));
  } catch (const std::exception& e) {
    logger.error(F("WebRouter"),
                 String(F("Hinzufügen der Middleware fehlgeschlagen: ")) + String(e.what()));
  }
}

String WebRouter::methodToString(HTTPMethod method) {
  switch (method) {
  case HTTP_GET:
    return "GET";
  case HTTP_POST:
    return "POST";
  case HTTP_PUT:
    return "PUT";
  case HTTP_DELETE:
    return "DELETE";
  case HTTP_HEAD:
    return "HEAD";
  case HTTP_OPTIONS:
    return "OPTIONS";
  case HTTP_PATCH:
    return "PATCH";
  case HTTP_ANY:
    return "ANY";
  default:
    return "UNKNOWN";
  }
}

bool WebRouter::executeMiddleware(HTTPMethod method, const String& url) {
  for (const auto& mw : _middleware) {
    logger.debug(F("WebRouter"),
                 String(F("Führe Middleware aus für: ")) + methodToString(method) + " " + url);
    if (!mw(method, url)) {
      logger.debug(F("WebRouter"), String(F("Middleware blockierte Anfrage: ")) + url);
      return false;
    }
  }
  return true;
}

Route* WebRouter::findRoute(HTTPMethod method, const String& url) {
  logger.debug(F("WebRouter"), F("Suche nach Route: ") + methodToString(method) + F(" ") + url);
  logger.debug(F("WebRouter"),
               String(F("Insgesamt registrierte Routen: ")) + String(_routes.size()));

  for (auto& route : _routes) {
    if (route.url == url && route.method == method) {
      logger.debug(F("WebRouter"),
                   F("Gefundene passende Route: ") + methodToString(method) + F(" ") + url);
      return &route;
    }
  }
  logger.warning(F("WebRouter"),
                 F("Keine passende Route gefunden für: ") + methodToString(method) + F(" ") + url);
  return nullptr;
}

void WebRouter::logRouteRegistration(HTTPMethod method, const String& url) {
  logger.debug(F("WebRouter"),
               String(F("Route erfolgreich registriert: ")) + methodToString(method) + " " + url);
}

RouterResult WebRouter::removeRoute(HTTPMethod method, const String& url) {
  auto it = std::remove_if(_routes.begin(), _routes.end(), [&](const Route& route) {
    return route.url == url && route.method == method;
  });

  if (it != _routes.end()) {
    _routes.erase(it, _routes.end());
    logger.debug(F("WebRouter"), F("Route entfernt: ") + methodToString(method) + F(" ") + url);
    return RouterResult::success();
  }

  logger.debug(F("WebRouter"),
               F("Route nicht gefunden zum Entfernen: ") + methodToString(method) + F(" ") + url);
  return RouterResult::fail(RouterError::INVALID_ROUTE, F("Route nicht gefunden"));
}

void WebRouter::removeHandlerRoutes(const String& handlerType) {
  if (handlerType.isEmpty()) {
    logger.debug(F("WebRouter"), F("Leerer handlerType - überspringe Route-Entfernung"));
    return;
  }

  auto it = std::remove_if(_routes.begin(), _routes.end(),
                           [&](const Route& route) { return route.handlerType == handlerType; });

  if (it != _routes.end()) {
    size_t removedCount = std::distance(it, _routes.end());
    _routes.erase(it, _routes.end());
    logger.info(F("WebRouter"), F("Handler-Routen entfernt: ") + handlerType + F(" (") +
                                    String(removedCount) + F(" Routen)"));
  } else {
    logger.debug(F("WebRouter"), F("Keine Routen gefunden für Handler: ") + handlerType);
  }
}
