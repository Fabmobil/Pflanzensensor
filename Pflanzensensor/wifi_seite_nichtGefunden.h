/**
 * @file wifi_seite_nichtGefunden.h
 * @brief Funktionen für die Ausgabe der 404-Seite
 *
 * Diese Datei enthält die Funktionen für die Ausgabe der 404-Seite.
 * Die 404-Seite wird angezeigt, wenn eine Seite nicht gefunden wurde.
 */

/* Funktion: WebseiteNichtGefundenAusgeben()
 * Gibt die 404-Seite des Webservers aus.
 */
void WebseiteNichtGefundenAusgeben() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebseiteNichtGefundenAusgeben()"));
  #endif

  Webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
  Webserver.send(404, F("text/html"), "");

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
