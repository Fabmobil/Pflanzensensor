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

namespace AdminEmail {
/**
 * @brief Text so ausgeben, dass er in HTML nichts kaputt macht
 * @details Der einzige HTML-Escaper im Projekt; deshalb hier im Header statt im
 *          anonymen Namensraum, damit die Vorlagenseite ihn mitbenutzen kann.
 *          Für Attributwerte und für den Inhalt einer textarea gleichermaßen
 *          geeignet - & und < sind abgedeckt.
 */
inline String maskiere(const String& roh) {
  String out;
  out.reserve(roh.length() + 8);
  for (size_t i = 0; i < roh.length(); i++) {
    const char c = roh[i];
    if (c == '&')
      out += F("&amp;");
    else if (c == '<')
      out += F("&lt;");
    else if (c == '>')
      out += F("&gt;");
    else if (c == '"')
      out += F("&quot;");
    else if (c == '\'')
      out += F("&#39;");
    else
      out += c;
  }
  return out;
}
} // namespace AdminEmail

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
  /// Vorlagenseite - in admin_email_handler_vorlagen.cpp
  void handleVorlagen();
  void handleVorlagenSave();

  void sendeSensorAuswahl();

  WebAuth& _auth;
  CSSService& _cssService;
};

#endif // ADMIN_EMAIL_HANDLER_H
