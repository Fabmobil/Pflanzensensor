// css_service.cpp
#include "web/services/css_service.h"

#include "logger/logger.h"
#include "managers/manager_resource.h"
#include "web/core/components.h"

CSSService::CSSService(ESPWebServer& server) : BaseHandler(server) {
  LOG_DEBUG(F("CSSService"), F("Initialisiere CSS-Service"));
  initModules();
}

void CSSService::initModules() {
  _modules = {CSSModule("base", "Base Styles", "/css/style.css"),
              CSSModule("start", "Start Page", "/css/start.css"),
              CSSModule("admin", "Admin Pages", "/css/admin.css"),
              CSSModule("logs", "Log Pages", "/css/logs.css")};
}

RouterResult CSSService::onRegisterRoutes(WebRouter& router) {
  LOG_INFO(F("CSSService"), F("CSS-Routen registriert"));
  return RouterResult::success();
}

HandlerResult CSSService::handleGet(const String& uri, const std::map<String, String>& query) {
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unknown endpoint");
}

HandlerResult CSSService::handlePost(const String& uri, const std::map<String, String>& params) {
  return HandlerResult::fail(HandlerError::NOT_FOUND, "Unknown endpoint");
}
