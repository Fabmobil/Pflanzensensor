/**
 * @file web_auth_decision.h
 * @brief Gedrosselte Protokollierung der Notfallpasswort-Nutzung
 * @details Eigene, hardwareunabhängige Datei aus demselben Grund wie
 *          utils/crc32.h: die Entscheidung "ist die letzte Warnung lange
 *          genug her" hängt an nichts Hardwarespezifischem.
 *
 *          Bewusst NICHT extrahiert: der eigentliche Passwortvergleich
 *          (server.authenticate()) bleibt in web_auth.cpp und läuft über die
 *          ESP8266WebServer-Bibliothek. Eine frühere Version dieses Projekts
 *          hatte den Basic-Auth-Header von Hand dekodiert (base64_decode())
 *          und wurde bewusst auf die Bibliotheksfunktion umgestellt, weil
 *          eine handgerollte Auth-Prüfung genau die Art sicherheitskritischen
 *          Codes ist, die man nicht ohne Not selbst pflegt. Diese Datei
 *          reimplementiert das nicht - sie testet nur die Drosselung, nicht
 *          den Passwortabgleich selbst.
 */

#ifndef WEB_AUTH_DECISION_H
#define WEB_AUTH_DECISION_H

#include <Arduino.h>

namespace WebAuthDecision {

/**
 * @brief Soll die Nutzung des Notfallpassworts jetzt protokolliert werden?
 * @param now Aktueller Zeitpunkt (millis())
 * @param lastWarnTime [in/out] Zeitpunkt der letzten Warnung, 0 = noch nie
 *        gewarnt. Wird bei true als Rückgabewert auf now aktualisiert.
 * @param intervalMs Mindestabstand zwischen zwei Warnungen
 * @return true, wenn diesmal gewarnt werden soll
 * @details Ohne Drosselung würde die Warnung das Log fluten: die Prüfung
 *          läuft pro Anfrage teils zweifach (Middleware und zusätzlich
 *          validateRequest im Handler), und beim Bedienen der Oberfläche
 *          entstehen viele Anfragen - dabei verlöre die Warnung ihre
 *          Signalwirkung.
 */
inline bool shouldWarnAboutEmergencyPassword(unsigned long now, unsigned long& lastWarnTime,
                                             unsigned long intervalMs) {
  if (lastWarnTime == 0 || now - lastWarnTime >= intervalMs) {
    lastWarnTime = now;
    return true;
  }
  return false;
}

} // namespace WebAuthDecision

#endif // WEB_AUTH_DECISION_H
