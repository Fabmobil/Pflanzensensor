/**
 * @file mail_vorlagen_standard.h
 * @brief Mitgelieferte Vorlagen und der mitgelieferte Stil
 * @details Sie liegen im PROGMEM und greifen immer dann, wenn die Datei
 *          /config/mailvorlagen.txt fehlt, eine fremde Kopfzeile hat oder den
 *          gesuchten Abschnitt nicht enthält. Dadurch braucht es beim ersten
 *          Start keinen Flash-Schreibvorgang, und "Auf Standard zurücksetzen"
 *          ist nichts weiter als das Löschen des Abschnitts.
 *
 *          Die Rümpfe sind Klartext mit einer winzigen Auszeichnung: `# ` macht
 *          eine Überschrift, `**` fettet, `[Text](Adresse)` verlinkt. Wer einen
 *          Satz ändern will, soll dafür kein HTML lesen müssen; die Gestaltung
 *          steht vollständig im Stil.
 *
 *          Der Stil wird beim Versand als <style>-Block in den Mailkopf
 *          geschrieben. Gmail wertet den aus, einige Programme ignorieren ihn -
 *          deshalb steht dort nichts, ohne das die Mail unlesbar würde: ohne
 *          jedes CSS bleibt eine schlichte, aber vollständige Nachricht.
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

const char BOOT_RUMPF[] PROGMEM = "# 🌱 Ich bin wieder wach!\n"
                                  "\n"
                                  "Moin! **{geraet}** hat neu gestartet und passt ab jetzt wieder "
                                  "auf die Pflanze auf. 🪴\n"
                                  "\n"
                                  "{messwerte}\n"
                                  "\n"
                                  "🔁 Neustart Nr. {neustarts} · ⏱️ läuft seit {laufzeit}\n"
                                  "📶 {ssid} · 🔗 [{name}](http://{name}) · {ip}\n"
                                  "🗓️ {datum} um {uhrzeit}\n";

const char WARNUNG_RUMPF[] PROGMEM = "# 🚨 Deine Pflanze braucht dich!\n"
                                     "\n"
                                     "Bei **{geraet}** ist gerade etwas nicht im grünen Bereich:\n"
                                     "\n"
                                     "{auffaellige}\n"
                                     "\n"
                                     "Alle Werte im Überblick:\n"
                                     "\n"
                                     "{messwerte}\n"
                                     "\n"
                                     "🔗 [Jetzt nachschauen](http://{name}) · {ip}\n"
                                     "🗓️ {datum} um {uhrzeit} · ⏱️ läuft seit {laufzeit}\n";

const char ALIVE_RUMPF[] PROGMEM = "# 🪴 Alles im grünen Bereich\n"
                                   "\n"
                                   "Lebenszeichen von **{geraet}** – läuft seit {laufzeit} ohne "
                                   "Zicken. 😎\n"
                                   "\n"
                                   "{messwerte}\n"
                                   "\n"
                                   "📶 {ssid} · 🔗 [{name}](http://{name}) · {ip}\n"
                                   "🗓️ {datum} um {uhrzeit} · 🔁 Neustart Nr. {neustarts}\n";

/**
 * Mitgelieferter Stil. Klassennamen, die der Nutzer kennen muss, stehen in der
 * Weboberfläche über dem Feld: body, h1, p, table.werte, .wert, .name,
 * .ampel-gruen/-gelb/-rot.
 */
const char STIL[] PROGMEM = "body{margin:0;padding:18px;background:#eef6e6;color:#213;\n"
                            "font-family:system-ui,Arial,sans-serif;font-size:15px;\n"
                            "line-height:1.5}\n"
                            "h1{margin:0 0 12px;font-size:26px}\n"
                            "p{margin:8px 0}\n"
                            "table.werte{width:100%;max-width:520px;border-collapse:collapse;\n"
                            "background:#fff;border-radius:8px;margin:12px 0}\n"
                            "table.werte td{padding:7px 10px;border-bottom:1px solid #e4eede}\n"
                            "td.wert{text-align:right;font-weight:600;white-space:nowrap}\n"
                            "a{color:#2a7}\n";

} // namespace MailVorlagenStandard

#endif // MAIL_VORLAGEN_STANDARD_H
