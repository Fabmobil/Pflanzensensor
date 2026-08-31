/**
 * @file mail_vorlagen_standard.h
 * @brief Mitgelieferte Vorlagen für die drei Mailarten
 * @details Sie liegen im PROGMEM und greifen immer dann, wenn die Datei
 *          /config/mailvorlagen.txt fehlt, eine fremde Kopfzeile hat oder den
 *          gesuchten Abschnitt nicht enthält. Dadurch braucht es beim ersten
 *          Start keinen Flash-Schreibvorgang, und "Auf Standard zurücksetzen"
 *          ist nichts weiter als das Löschen des Abschnitts.
 *
 *          Die Gestaltung steckt in Inline-Styles, nicht in einem
 *          <style>-Block: Gmail und Outlook entfernen den Kopfbereich
 *          teilweise. Tabellenlayout aus demselben Grund - Flexbox und Grid
 *          sind in Mailprogrammen unzuverlässig.
 */

#ifndef MAIL_VORLAGEN_STANDARD_H
#define MAIL_VORLAGEN_STANDARD_H

#include <Arduino.h>

namespace MailVorlagenStandard {

// Betreffzeilen. Emojis vorn, weil in der Übersicht des Postfachs nur die
// ersten Zeichen zu sehen sind.
const char BOOT_BETREFF[] PROGMEM = "🌱 {geraet} ist wieder wach!";
const char WARNUNG_BETREFF[] PROGMEM = "🚨 {geraet}: da stimmt was nicht!";
const char ALIVE_BETREFF[] PROGMEM = "🪴 {geraet} meldet sich – alles gut ({datum})";

const char BOOT_RUMPF[] PROGMEM =
    "<html><body style=\"margin:0;background:#eef6e6;font-family:system-ui,Arial,sans-serif;"
    "color:#213\">\n"
    "<div style=\"max-width:520px;margin:0 auto;padding:18px\">\n"
    "<h1 style=\"margin:0;font-size:26px\">🌱 Ich bin wieder wach!</h1>\n"
    "<p style=\"font-size:15px;line-height:1.5\">Moin! <b>{geraet}</b> hat neu gestartet und passt "
    "ab jetzt wieder auf die Pflanze auf. 🪴</p>\n"
    "<table style=\"width:100%;border-collapse:collapse;font-size:15px\">\n"
    "{messwerte}\n"
    "</table>\n"
    "<p style=\"font-size:13px;color:#567;line-height:1.6\">🔁 Neustart Nr. {neustarts} &middot; "
    "⏱️ {laufzeit}<br>\n"
    "📶 {ssid} &middot; 🔗 <a href=\"http://{ip}\" style=\"color:#2a7\">{ip}</a><br>\n"
    "🗓️ {datum} um {uhrzeit}</p>\n"
    "</div></body></html>\n";

const char WARNUNG_RUMPF[] PROGMEM =
    "<html><body style=\"margin:0;background:#fff4e5;font-family:system-ui,Arial,sans-serif;"
    "color:#213\">\n"
    "<div style=\"max-width:520px;margin:0 auto;padding:18px\">\n"
    "<h1 style=\"margin:0;font-size:26px\">🚨 Deine Pflanze braucht dich!</h1>\n"
    "<p style=\"font-size:15px;line-height:1.5\">Bei <b>{geraet}</b> ist gerade etwas nicht im "
    "grünen Bereich:</p>\n"
    "<table style=\"width:100%;border-collapse:collapse;font-size:17px;background:#fff;"
    "border-radius:8px\">\n"
    "{auffaellige}\n"
    "</table>\n"
    "<p style=\"font-size:14px;margin-top:18px\">Alle Werte im Überblick:</p>\n"
    "<table style=\"width:100%;border-collapse:collapse;font-size:14px;color:#567\">\n"
    "{messwerte}\n"
    "</table>\n"
    "<p style=\"font-size:13px;color:#567;line-height:1.6\">🔗 <a href=\"http://{ip}\" "
    "style=\"color:#2a7\">Jetzt nachschauen</a><br>\n"
    "🗓️ {datum} um {uhrzeit} &middot; ⏱️ läuft seit {laufzeit}</p>\n"
    "</div></body></html>\n";

const char ALIVE_RUMPF[] PROGMEM =
    "<html><body style=\"margin:0;background:#eef6e6;font-family:system-ui,Arial,sans-serif;"
    "color:#213\">\n"
    "<div style=\"max-width:520px;margin:0 auto;padding:18px\">\n"
    "<h1 style=\"margin:0;font-size:26px\">🪴 Alles im grünen Bereich</h1>\n"
    "<p style=\"font-size:15px;line-height:1.5\">Lebenszeichen von <b>{geraet}</b> – läuft seit "
    "{laufzeit} ohne Zicken. 😎</p>\n"
    "<table style=\"width:100%;border-collapse:collapse;font-size:15px\">\n"
    "{messwerte}\n"
    "</table>\n"
    "<p style=\"font-size:13px;color:#567;line-height:1.6\">📶 {ssid} &middot; 🔗 "
    "<a href=\"http://{ip}\" style=\"color:#2a7\">{ip}</a><br>\n"
    "🗓️ {datum} um {uhrzeit} &middot; 🔁 Neustart Nr. {neustarts}</p>\n"
    "</div></body></html>\n";

} // namespace MailVorlagenStandard

#endif // MAIL_VORLAGEN_STANDARD_H
