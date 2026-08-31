/**
 * @file admin_email_handler.h
 * @brief Konfigurationsseite für den Mailversand
 * @details Drei Routen: die Seite, das Speichern des Formulars und eine
 *          Abfrage für das Ergebnis der Testmail. Kein Singleton, wie beim
 *          Chronik- und Startseitenhandler.
 */

#ifndef ADMIN_EMAIL_HANDLER_H
#define ADMIN_EMAIL_HANDLER_H

#include "web/core/web_auth.h"
#include "web/handler/base_handler.h"
#include "web/services/css_service.h"

class AdminEmailHandler : public BaseHandler {
public:
  AdminEmailHandler(ESPWebServer& server, WebAuth& auth, CSSService& cssService)
      : BaseHandler(server), _auth(auth), _cssService(cssService) {
    LOG_DEBUG(F("AdminEmail"), F("Initialisiere AdminEmailHandler"));
  }

  /// Muss synchron zu onRegisterRoutes() bleiben - siehe Lazy-Loading im WebManager.
  static bool ownsUrl(const String& url);

  RouterResult onRegisterRoutes(WebRouter& router) override;

protected:
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

private:
  void handlePage();
  void handleSave();
  void handleTest();
  void handleStatus();

  void sendeSensorAuswahl();

  WebAuth& _auth;
  CSSService& _cssService;
};

#endif // ADMIN_EMAIL_HANDLER_H
