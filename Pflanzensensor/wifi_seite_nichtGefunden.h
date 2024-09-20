/**
 * @file wifi_seite_nichtGefunden.h
 * @brief 404-Fehlerseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält Funktionen zur Generierung der 404-Fehlerseite
 * des Pflanzensensors.
 */

#ifndef WIFI_SEITE_NICHT_GEFUNDEN_H
#define WIFI_SEITE_NICHT_GEFUNDEN_H

/**
 * @brief Generiert und sendet die 404 (Nicht gefunden) Fehlerseite
 *
 * Diese Funktion wird aufgerufen, wenn eine angeforderte Seite nicht gefunden wurde.
 * Sie sendet eine entsprechende Fehlermeldung an den Client.
 */
void WebseiteNichtGefundenAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebseiteNichtGefundenAusgeben()"));
  #endif

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(404, F("text/html"), "");

  Webserver.sendContent_P(htmlHeaderNoRefresh);
  Webserver.sendContent_P(htmlHeader);

  Webserver.sendContent(F(
    "<h2>404 - Seite nicht gefunden</h2>"
    "<p>Die angeforderte Seite konnte nicht gefunden werden.</p>"
    "<p>Bitte überprüfe die URL und versuche es erneut.</p>"
    "<p><a href=\"/\">Zurück zur Startseite</a></p>"
  ));

  Webserver.sendContent_P(htmlFooter);
  Webserver.client().flush();

  #if MODUL_DEBUG
    Serial.println(F("# Ende von WebseiteNichtGefundenAusgeben()"));
  #endif
}

#endif // WIFI_SEITE_NICHT_GEFUNDEN_H
