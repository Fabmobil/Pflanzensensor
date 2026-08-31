/**
 * @file chronik_handler.h
 * @brief Seite und Datenschnittstelle der Chronik
 * @details Drei Routen: die Seite selbst, der Rohdatenstrom für das Diagramm
 *          und ein CSV-Export für Menschen.
 *
 *          Kein Singleton wie der LogHandler - dessen Einzelinstanz gibt es
 *          nur, weil Logger und WebSocket von außen darauf zugreifen. Hier
 *          reicht das übliche Muster mit std::make_unique im Handler-Cache.
 */

#ifndef CHRONIK_HANDLER_H
#define CHRONIK_HANDLER_H

#include "web/core/request_throttle.h"
#include "web/core/web_auth.h"
#include "web/handler/base_handler.h"
#include "web/services/css_service.h"

class ChronikHandler : public BaseHandler {
public:
  /// Mindestabstand zwischen zwei CSV-Exporten.
  ///
  /// Der Export formatiert zehntausende Fließkommazahlen zu Text und ist damit
  /// der teuerste Endpunkt des Geräts - auf einem Server, der jeweils nur eine
  /// Verbindung bedient. Die Seite selbst ist ohne Anmeldung erreichbar,
  /// deshalb die Bremse.
  static constexpr uint32_t EXPORT_MIN_INTERVAL_MS = 10000;

  ChronikHandler(ESPWebServer& server, WebAuth& auth, CSSService& cssService)
      : BaseHandler(server), _auth(auth), _cssService(cssService) {
    LOG_DEBUG(F("Chronik"), F("Initialisiere ChronikHandler"));
  }

  /**
   * @brief Gehört diese URL zu diesem Handler?
   * @details Wird von der Lazy-Loading-Middleware benutzt und muss synchron zu
   *          onRegisterRoutes() bleiben - deshalb steht beides beieinander.
   */
  static bool ownsUrl(const String& url);

  RouterResult onRegisterRoutes(WebRouter& router) override;

protected:
  HandlerResult handleGet(const String& uri, const std::map<String, String>& query) override;
  HandlerResult handlePost(const String& uri, const std::map<String, String>& params) override;

private:
  void handlePage();
  void handleData();
  void handleExport();

  WebAuth& _auth;
  CSSService& _cssService;
};

#endif // CHRONIK_HANDLER_H
