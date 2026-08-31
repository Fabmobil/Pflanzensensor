/**
 * @file admin_email_handler_vorlagen.cpp
 * @brief Bearbeitungsseite für die Mailvorlagen
 * @details Eigene Seite und nicht Teil des Mailformulars: ein gemeinsames
 *          Formular schickte Zugangsdaten und Vorlagentext in einem POST, und
 *          der Webserver hält den urlencodierten Rumpf dreifach im Heap
 *          (Parsing-impl.h:160-180). Jede Vorlage bekommt deshalb ihren
 *          eigenen, kleinen POST.
 */

#include "web/handler/admin_email_handler.h"

#include "logger/logger.h"
#include "mail/mail_sender.h"
#include "mail/mail_vorlagen.h"
#include "managers/manager_config.h"
#include "web/core/components.h"

using AdminEmail::maskiere;
using MailVorlage::Abschnitt;

namespace {

/// Zum Anzeigen genügt weniger als zum Speichern - Lesen streamt, Speichern
/// nicht.
constexpr uint32_t MIN_HEAP_SEITE = 6000;
/// Der POST trägt bis zu 1720 Byte, urlencodiert bis zu 4,5 KB, und der
/// Webserver hält das dreifach. Darunter gar nicht erst anfangen.
constexpr uint32_t MIN_HEAP_SPEICHERN = 12000;

struct Art {
  const char* schluessel;
  const char* titel;
  Mail::Kind kind;
};

const Art ARTEN[] = {{"boot", "Startmeldung", Mail::Kind::Boot},
                     {"warnung", "Warnung", Mail::Kind::Warning},
                     {"alive", "Lebenszeichen", Mail::Kind::Alive}};

const Art& artVon(const String& schluessel) {
  for (const Art& a : ARTEN) {
    if (schluessel == a.schluessel) {
      return a;
    }
  }
  return ARTEN[0];
}

/// Zeilen aus der Vorlage direkt in die Seite schreiben - so liegt der Rumpf
/// nie ganz im Heap.
struct Ausgabe {
  ESPWebServer* server;
  bool ersteZeile;
};

} // namespace

void AdminEmailHandler::handleVorlagen() {
  if (ESP.getFreeHeap() < MIN_HEAP_SEITE) {
    _server.send(200, F("text/html"),
                 F("<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><title>Vorlagen"
                   "</title></head><body><h1>Mailtexte</h1><p>Gerade zu wenig Speicher.</p>"
                   "<p><a href='/admin/email'>Zurück</a></p></body></html>"));
    return;
  }

  const Art& art = artVon(_server.arg("art"));

  const std::vector<String> css = {};
  const std::vector<String> js = {"mailvorlagen"};

  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/email",
      [this, &art]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>\xE2\x9C\x8F\xEF\xB8\x8F Mailtexte</h2>"));

        // Reiter
        sendChunk(F("<div class='chronik-ranges' style='margin-bottom:0.8em'>"));
        for (const Art& a : ARTEN) {
          sendChunk(F("<a class='"));
          sendChunk(&a == &art ? F("button-primary") : F("button-secondary"));
          sendChunk(F("' href='/admin/email/vorlagen?art="));
          sendChunk(a.schluessel);
          sendChunk(F("'>"));
          sendChunk(a.titel);
          sendChunk(F("</a> "));
        }
        sendChunk(F("</div>"));

        // Rückmeldung des letzten Speicherversuchs
        const String fehler = _server.arg("fehler");
        if (fehler.length()) {
          sendChunk(F("<div class='mail-hinweis' style='color:#ff6f6f'>\xE2\x9A\xA0\xEF\xB8\x8F "));
          if (fehler == "zeile") {
            sendChunk(F("Nicht gespeichert: eine Zeile ist länger als 256 Bytes. Teile sie auf - "
                        "im HTML ist ein Zeilenumbruch zwischen Tags bedeutungslos."));
          } else if (fehler == "laenge") {
            sendChunk(F("Nicht gespeichert: der Text ist länger als 1600 Bytes."));
          } else if (fehler == "betreff") {
            sendChunk(F("Nicht gespeichert: der Betreff ist länger als 120 Bytes."));
          } else if (fehler == "speicher") {
            sendChunk(F("Nicht gespeichert: gerade zu wenig Arbeitsspeicher. Bitte gleich noch "
                        "einmal versuchen."));
          } else {
            sendChunk(F("Nicht gespeichert - siehe Logs."));
          }
          sendChunk(F("</div>"));
        }

        sendChunk(F("<form method='post' action='/admin/email/vorlagen/save' id='vorlage_form'>"));
        sendChunk(F("<input type='hidden' name='art' value='"));
        sendChunk(art.schluessel);
        sendChunk(F("'>"));

        sendChunk(F("<div><label>Betreff<br><input type='text' name='betreff' id='vorlage_betreff' "
                    "style='width:100%' value='"));
        {
          char betreffRoh[MailVorlage::BETREFF_MAX * 2 + 1];
          betreffRoh[0] = '\0';
          struct Ziel {
            char* out;
            size_t outSize;
            bool fertig;
          } ziel{betreffRoh, sizeof(betreffRoh), false};
          MailVorlagen::sendeRoh(
              MailVorlagen::abschnittFuer(art.kind, true),
              [](const char* zeile, void* context) {
                Ziel* z = static_cast<Ziel*>(context);
                if (z->fertig)
                  return;
                strncpy(z->out, zeile, z->outSize - 1);
                z->out[z->outSize - 1] = '\0';
                z->fertig = true;
              },
              &ziel);
          sendChunk(maskiere(betreffRoh));
        }
        sendChunk(F("'></label></div>"));

        sendChunk(F("<div style='margin-top:0.6em'><label>Text der Mail (HTML)<br>"
                    "<textarea name='rumpf' id='vorlage_rumpf' rows='16' "
                    "style='width:100%;font-family:monospace;font-size:0.85em'>"));
        {
          Ausgabe a{&_server, true};
          MailVorlagen::sendeRoh(
              MailVorlagen::abschnittFuer(art.kind, false),
              [](const char* zeile, void* context) {
                Ausgabe* o = static_cast<Ausgabe*>(context);
                if (!o->ersteZeile) {
                  Component::sendChunk(*o->server, F("\n"));
                }
                o->ersteZeile = false;
                Component::sendChunk(*o->server, maskiere(zeile));
              },
              &a);
        }
        sendChunk(F("</textarea></label></div>"));

        sendChunk(F("<div class='mail-hinweis' id='vorlage_zaehler'>&nbsp;</div>"));

        // Platzhalter zum Anklicken
        sendChunk(F("<div class='mail-hinweis'>Platzhalter einfügen:</div>"));
        sendChunk(F("<div class='mail-sensorliste' id='vorlage_platzhalter'>"));
        const char* einfache[] = {"geraet",   "ip",    "ssid",   "neustarts",
                                  "laufzeit", "datum", "uhrzeit"};
        for (const char* name : einfache) {
          sendChunk(F("<button type='button' class='button-secondary' data-ph='{"));
          sendChunk(name);
          sendChunk(F("}'>{"));
          sendChunk(name);
          sendChunk(F("}</button>"));
        }
        sendChunk(F("</div>"));
        sendChunk(F("<div class='mail-sensorliste'>"));
        const char* bloecke[] = {MailVorlage::BLOCK_MESSWERTE, MailVorlage::BLOCK_AUFFAELLIGE};
        for (const char* name : bloecke) {
          sendChunk(F("<button type='button' class='button-primary' data-ph='{"));
          sendChunk(name);
          sendChunk(F("}'>{"));
          sendChunk(name);
          sendChunk(F("}</button>"));
        }
        sendChunk(F("</div>"));
        sendChunk(F("<div class='mail-hinweis'>Die beiden farbigen Platzhalter setzen ganze "
                    "Tabellenzeilen ein und müssen deshalb allein in einer Zeile stehen.</div>"));

        sendChunk(F("<div style='margin-top:1em'>"
                    "<button type='submit' class='button-primary'>Speichern</button> "
                    "<button type='submit' name='was' value='reset' class='button-secondary'>"
                    "Auf Standard zurücksetzen</button></div>"));
        sendChunk(F("</form>"));

        sendChunk(F("<div class='mail-hinweis' id='vorlage_befunde'></div>"));

        // Vorschau mit den echten Werten des Geräts. Die Vorschau im Browser
        // arbeitet mit Beispielwerten; hier sieht man, was tatsächlich in der
        // Mail steht - inklusive Messwerttabelle, SSID und Neustartzähler.
        sendChunk(F("<div style='margin-top:0.8em'><a class='button-secondary' href='"
                    "/admin/email/vorlagen?art="));
        sendChunk(art.schluessel);
        sendChunk(F("&geraetevorschau=1'>\xF0\x9F\x94\x8E Vorschau mit echten Messwerten"
                    "</a></div>"));

        if (_server.arg("geraetevorschau") == "1") {
          sendChunk(F("<h3>So sieht die Mail gerade aus</h3>"));
          sendChunk(F("<pre style='white-space:pre-wrap;font-size:0.75em;background:#1c1c1c;"
                      "padding:8px;border-radius:6px;overflow-x:auto'>"));
          char betreffEcht[MailVorlage::BETREFF_MAX + 1];
          MailVorlagen::betreff(art.kind, betreffEcht, sizeof(betreffEcht));
          sendChunk(F("Betreff: "));
          sendChunk(maskiere(betreffEcht));
          sendChunk(F("\n\n"));

          Ausgabe echt{&_server, true};
          MailVorlagen::sendeRumpf(
              art.kind,
              [](const char* text, size_t length, void* context) {
                Ausgabe* o = static_cast<Ausgabe*>(context);
                String zeile;
                zeile.reserve(length + 1);
                for (size_t i = 0; i < length; i++) {
                  zeile += text[i];
                }
                Component::sendChunk(*o->server, maskiere(zeile));
                Component::sendChunk(*o->server, F("\n"));
              },
              &echt);
          sendChunk(F("</pre>"));
        }
        sendChunk(F("<h3>Vorschau</h3>"));
        sendChunk(F("<iframe id='vorlage_vorschau' sandbox='' "
                    "style='width:100%;height:340px;border:1px solid #444;border-radius:6px;"
                    "background:#fff'></iframe>"));
        sendChunk(F("<div class='mail-hinweis'>Die Vorschau setzt Beispielwerte ein. Die "
                    "Messwerttabelle sieht in der echten Mail nach deinen Sensoren aus.</div>"));
        sendChunk(F("</div>"));
      },
      css, js);
}

void AdminEmailHandler::handleVorlagenSave() {
  const Art& art = artVon(_server.arg("art"));
  const bool zuruecksetzen = (_server.arg("was") == "reset");

  String ziel = String(F("/admin/email/vorlagen?art=")) + art.schluessel;
  String fehler;

  if (zuruecksetzen) {
    if (!MailVorlagen::setzeZurueck(art.kind, fehler)) {
      LOG_ERROR(F("AdminEmail"), String(F("Zuruecksetzen fehlgeschlagen: ")) + fehler);
      ziel += F("&fehler=1");
    }
    sendRedirect(ziel);
    return;
  }

  // KEIN MemoryMgr.emergencyCleanup() hier, so verlockend es wäre: die
  // Aufräumfunktion leert den Handler-Cache des Webservers - und darin steckt
  // genau dieser Handler. Nach dem Aufruf zeigt "this" ins Leere, und der
  // nächste Zugriff auf _server endet in "Fatal exception 28". Der MailSender
  // darf das, weil er aus loop() läuft und dabei kein Handler auf dem Stack
  // liegt.
  if (ESP.getFreeHeap() < MIN_HEAP_SPEICHERN) {
    LOG_ERROR(F("AdminEmail"),
              String(F("Zu wenig Speicher zum Speichern: ")) + String(ESP.getFreeHeap()));
    sendRedirect(ziel + F("&fehler=speicher"));
    return;
  }

  const String betreffText = _server.arg("betreff");
  const String rumpfText = _server.arg("rumpf");

  // Ablehnen statt abschneiden: ein abgeschnittenes HTML zerreißt mitten im
  // Tag, und ein abgeschnittenes Emoji ergibt ein Ersatzzeichen.
  if (betreffText.length() > MailVorlage::BETREFF_MAX) {
    sendRedirect(ziel + F("&fehler=betreff"));
    return;
  }
  if (rumpfText.length() > MailVorlage::RUMPF_MAX) {
    sendRedirect(ziel + F("&fehler=laenge"));
    return;
  }

  // Einzelne Zeilen dürfen nicht länger sein als der Lesepuffer. Sonst liest
  // das Gerät sie beim nächsten Öffnen in Stücken, und aus einer Zeile würden
  // beim erneuten Speichern dauerhaft mehrere. Lieber jetzt ablehnen und
  // sagen, warum.
  {
    int start = 0;
    while (start <= static_cast<int>(rumpfText.length())) {
      const int ende = rumpfText.indexOf('\n', start);
      const int laenge = ((ende < 0) ? rumpfText.length() : ende) - start;
      if (laenge > static_cast<int>(MailVorlage::ZEILE_MAX)) {
        sendRedirect(ziel + F("&fehler=zeile"));
        return;
      }
      if (ende < 0) {
        break;
      }
      start = ende + 1;
    }
  }

  if (!MailVorlagen::speichere(art.kind, betreffText, rumpfText, fehler)) {
    LOG_ERROR(F("AdminEmail"), String(F("Vorlage nicht gespeichert: ")) + fehler);
    ziel += F("&fehler=1");
  }
  sendRedirect(ziel);
}
