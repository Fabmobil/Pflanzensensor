/**
 * @file  wifi_seite_nichtGefunden.h
 * @brief Funktionen für die Ausgabe der 404-Seite
 *
 * Diese Datei enthält die Funktionen für die Ausgabe der 404-Seite.
 * Die 404-Seite wird angezeigt, wenn eine Seite nicht gefunden wurde.
 */


/* Funktion: WebseiteNichtGefundenAusgeben()
 * Gibt die 404-Seite des Webservers aus.
 */
void WebseiteNichtGefundenAusgeben() {
  #include "wifi_header.h"
  #include "wifi_footer.h"
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebseiteNichtGefundenAusgeben()"));
  #endif
  String formatierterCode = htmlHeader;
  formatierterCode += "<h2>404 - Seite nicht gefunden</h2>";
  formatierterCode += "<p>Die angeforderte Seite konnte nicht gefunden werden.</p>";
  formatierterCode += "<p>Bitte überprüfe die URL und versuche es erneut.</p>";
  formatierterCode += "<p><a href=\"/\">Zurück zur Startseite</a></p>";
  formatierterCode += htmlFooter;
  Webserver.send(404, "text/html", formatierterCode);
}
