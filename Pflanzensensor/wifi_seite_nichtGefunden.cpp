/**
 * @file wifi_seite_nichtGefunden.cpp
 * @brief Implementierung der 404-Fehlerseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Implementierung der Funktion zur Generierung der 404-Fehlerseite
 * des Pflanzensensors.
 */

#include "einstellungen.h"
#include "passwoerter.h"
#include "wifi_seite_nichtGefunden.h"
#include "logger.h"
#include "wifi.h"
#include "wifi_header.h"
#include "wifi_footer.h"

void WebseiteNichtGefundenAusgeben() {
  logger.debug(F("Beginn von WebseiteNichtGefundenAusgeben()"));

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(404, F("text/html"), "");

  sendeHtmlHeader(Webserver, false);

  Webserver.sendContent(F(
    "<h2>404 - Seite nicht gefunden</h2>"
    "<p>Die angeforderte Seite konnte nicht gefunden werden.</p>"
    "<p>Bitte überprüfe die URL und versuche es erneut.</p>"
    "<p><a href=\"/\">Zurück zur Startseite</a></p>"
  ));

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();

  logger.debug(F("Ende von WebseiteNichtGefundenAusgeben()"));
}
