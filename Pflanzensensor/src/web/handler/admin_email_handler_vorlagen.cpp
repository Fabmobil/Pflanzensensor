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
#include "utils/memory_manager.h"
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

const Art ARTEN[] = {{"boot", "\xF0\x9F\x8C\xB1 Startmeldung", Mail::Kind::Boot},
                     {"warnung", "\xF0\x9F\x9A\xA8 Warnung", Mail::Kind::Warning},
                     {"alive", "\xF0\x9F\xAA\xB4 Lebenszeichen", Mail::Kind::Alive}};

/// Vierter Reiter: das gemeinsame CSS. Kein eigener Art-Eintrag, weil dahinter
/// keine Mailart steht - und keine eigene Route, weil das Routenbudget knapp
/// ist (web_router.h:74).
constexpr const char* STIL_SCHLUESSEL = "stil";

/// Emojis zum Anklicken. Kuratiert statt vollständig: eine vollständige Liste
/// wären ein paar Kilobyte, die über jede Vorlagenseite gingen.
const char* const EMOJIS[] = {"\xF0\x9F\x8C\xB1",
                              "\xF0\x9F\xAA\xB4",
                              "\xF0\x9F\x8C\xB8",
                              "\xF0\x9F\x8C\xBB",
                              "\xF0\x9F\x8D\x80",
                              "\xF0\x9F\x8C\xB5",
                              "\xF0\x9F\x8D\x83",
                              "\xF0\x9F\x92\xA7",
                              "\xF0\x9F\x92\xA6",
                              "\xE2\x98\x80\xEF\xB8\x8F",
                              "\xF0\x9F\x8C\xA7\xEF\xB8\x8F",
                              "\xF0\x9F\x8C\xA1\xEF\xB8\x8F",
                              "\xF0\x9F\x94\x86",
                              "\xF0\x9F\x9F\xA2",
                              "\xF0\x9F\x9F\xA1",
                              "\xF0\x9F\x94\xB4",
                              "\xE2\x9A\xA0\xEF\xB8\x8F",
                              "\xF0\x9F\x9A\xA8",
                              "\xE2\x9C\x85",
                              "\xE2\x9D\x97",
                              "\xF0\x9F\x98\x8E",
                              "\xF0\x9F\xA5\xB3",
                              "\xF0\x9F\x98\x85",
                              "\xF0\x9F\x98\xB1",
                              "\xF0\x9F\x98\x8D",
                              "\xF0\x9F\x91\x8B",
                              "\xF0\x9F\x91\x80",
                              "\xF0\x9F\x92\xAA",
                              "\xF0\x9F\x8E\x89",
                              "\xF0\x9F\x93\xB6",
                              "\xF0\x9F\x94\x97",
                              "\xF0\x9F\x94\x81",
                              "\xE2\x8F\xB1\xEF\xB8\x8F",
                              "\xF0\x9F\x97\x93\xEF\xB8\x8F",
                              "\xF0\x9F\x93\x8A",
                              "\xF0\x9F\xA4\x96"};

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

/// @brief Betreff, Text, Emojis und Platzhalter - die drei Mailarten
void sendeEmojis(ESPWebServer& server);

/**
 * @brief Ist jede Zeile kurz genug?
 * @details Einzelne Zeilen dürfen nicht länger sein als der Lesepuffer. Sonst
 *          liest das Gerät sie beim nächsten Öffnen in Stücken, und aus einer
 *          Zeile würden beim erneuten Speichern dauerhaft mehrere. Lieber jetzt
 *          ablehnen und sagen, warum.
 */
bool pruefeZeilenlaenge(const String& text) {
  int start = 0;
  while (start <= static_cast<int>(text.length())) {
    const int ende = text.indexOf('\n', start);
    const int laenge = ((ende < 0) ? text.length() : ende) - start;
    if (laenge > static_cast<int>(MailVorlage::ZEILE_MAX)) {
      return false;
    }
    if (ende < 0) {
      break;
    }
    start = ende + 1;
  }
  return true;
}

void sendeTextFelder(ESPWebServer& server, const Art& art) {
  Component::sendChunk(
      server, F("<div><label>Betreff<br><input type='text' name='betreff' id='vorlage_betreff' "
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
    Component::sendChunk(server, maskiere(betreffRoh));
  }
  Component::sendChunk(server, F("'></label></div>"));

  Component::sendChunk(server, F("<div style='margin-top:0.6em'><label>Text der Mail<br>"
                                 "<textarea name='rumpf' id='vorlage_rumpf' rows='14' "
                                 "style='width:100%;font-size:0.95em'>"));
  {
    Ausgabe a{&server, true};
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
  Component::sendChunk(server, F("</textarea></label></div>"));

  Component::sendChunk(server, F("<div class='mail-hinweis' id='vorlage_zaehler'>&nbsp;</div>"));

  sendeEmojis(server);

  Component::sendChunk(server, F("<div class='mail-hinweis'>Platzhalter einfügen:</div>"));
  Component::sendChunk(server, F("<div class='mail-sensorliste' id='vorlage_platzhalter'>"));
  const char* einfache[] = {"geraet",    "name",     "ip",    "ssid",
                            "neustarts", "laufzeit", "datum", "uhrzeit"};
  for (const char* name : einfache) {
    Component::sendChunk(server, F("<button type='button' class='button-secondary' data-ph='{"));
    Component::sendChunk(server, name);
    Component::sendChunk(server, F("}'>{"));
    Component::sendChunk(server, name);
    Component::sendChunk(server, F("}</button>"));
  }
  Component::sendChunk(server, F("</div>"));
  Component::sendChunk(server, F("<div class='mail-sensorliste'>"));
  const char* bloecke[] = {MailVorlage::BLOCK_MESSWERTE, MailVorlage::BLOCK_AUFFAELLIGE};
  for (const char* name : bloecke) {
    Component::sendChunk(server, F("<button type='button' class='button-primary' data-ph='{"));
    Component::sendChunk(server, name);
    Component::sendChunk(server, F("}'>{"));
    Component::sendChunk(server, name);
    Component::sendChunk(server, F("}</button>"));
  }
  Component::sendChunk(server, F("</div>"));
  Component::sendChunk(
      server, F("<div class='mail-hinweis'>Die beiden farbigen Platzhalter setzen eine ganze "
                "Tabelle ein und müssen deshalb allein in einer Zeile stehen.<br>"
                "Zum Gestalten: <b># </b> am Zeilenanfang macht eine Überschrift, "
                "<b>**Text**</b> macht fett, <b>[Text](http://…)</b> macht einen Link. "
                "Alles andere ist einfach Text - wie es aussieht, steht unter "
                "\xF0\x9F\x8E\xA8 Design.</div>"));
}

/// @brief Das gemeinsame CSS
void sendeStilFeld(ESPWebServer& server) {
  Component::sendChunk(
      server, F("<div class='mail-hinweis'>Dieses Design gilt für alle drei Mails. Es wird beim "
                "Versand in den Kopf der Mail geschrieben. Manche Mailprogramme werfen fremdes "
                "CSS weg - dann kommt die Mail schlicht, aber vollständig an.</div>"));

  Component::sendChunk(server, F("<div style='margin-top:0.6em'><label>Design (CSS)<br>"
                                 "<textarea name='css' id='vorlage_rumpf' rows='16' "
                                 "style='width:100%;font-family:monospace;font-size:0.85em'>"));
  {
    Ausgabe a{&server, true};
    MailVorlagen::sendeStil(
        [](const char* text, size_t length, void* context) {
          Ausgabe* o = static_cast<Ausgabe*>(context);
          if (!o->ersteZeile) {
            Component::sendChunk(*o->server, F("\n"));
          }
          o->ersteZeile = false;
          String zeile;
          zeile.reserve(length + 1);
          for (size_t i = 0; i < length; i++) {
            zeile += text[i];
          }
          Component::sendChunk(*o->server, maskiere(zeile));
        },
        &a);
  }
  Component::sendChunk(server, F("</textarea></label></div>"));

  Component::sendChunk(server, F("<div class='mail-hinweis' id='vorlage_zaehler'>&nbsp;</div>"));
  Component::sendChunk(
      server, F("<div class='mail-hinweis'>Diese Bausteine kannst du ansprechen:<br>"
                "<b>body</b> die ganze Mail &middot; <b>h1</b> die Überschrift &middot; "
                "<b>p</b> ein Absatz &middot; <b>a</b> ein Link &middot; "
                "<b>table.werte</b> die Werttabelle &middot; <b>td.name</b> die Bezeichnung "
                "&middot; <b>td.wert</b> die Zahl</div>"));
}

/// @brief Emojis zum Anklicken
void sendeEmojis(ESPWebServer& server) {
  Component::sendChunk(server, F("<div class='mail-hinweis'>Emoji einfügen:</div>"));
  Component::sendChunk(server, F("<div class='mail-sensorliste' id='vorlage_emojis'>"));
  for (const char* e : EMOJIS) {
    Component::sendChunk(
        server, F("<button type='button' class='button-secondary' "
                  "style='padding:0.25em 0.45em;margin:0.15em;font-size:1.15em' data-emoji='"));
    Component::sendChunk(server, e);
    Component::sendChunk(server, F("'>"));
    Component::sendChunk(server, e);
    Component::sendChunk(server, F("</button>"));
  }
  Component::sendChunk(server, F("</div>"));
}

} // namespace

void AdminEmailHandler::handleVorlagen() {
  if (ESP.getFreeHeap() < MIN_HEAP_SEITE) {
    _server.send(200, F("text/html"),
                 F("<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><title>Vorlagen"
                   "</title></head><body><h1>Mailtexte</h1><p>Gerade zu wenig Speicher.</p>"
                   "<p><a href='/admin/email'>Zurück</a></p></body></html>"));
    return;
  }

  const String artArg = _server.arg("art");
  const bool stilSeite = (artArg == STIL_SCHLUESSEL);
  const Art& art = artVon(artArg);

  const std::vector<String> css = {};
  const std::vector<String> js = {"mailvorlagen"};

  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/email",
      [this, &art, stilSeite]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>\xE2\x9C\x8F\xEF\xB8\x8F Mailtexte</h2>"));

        // Reiter. Die Klasse "button" muss mit: die Grundregel in admin.css
        // greift nur bei button, input und .button - ein <a> allein mit
        // .button-secondary bekäme weder Polsterung noch Rundung.
        sendChunk(F("<div class='chronik-ranges' style='margin-bottom:0.8em;flex-wrap:wrap'>"));
        for (const Art& a : ARTEN) {
          sendChunk(F("<a class='button "));
          sendChunk((&a == &art && !stilSeite) ? F("button-primary") : F("button-secondary"));
          sendChunk(F("' href='/admin/email/vorlagen?art="));
          sendChunk(a.schluessel);
          sendChunk(F("'>"));
          sendChunk(a.titel);
          sendChunk(F("</a>"));
        }
        sendChunk(F("<a class='button "));
        sendChunk(stilSeite ? F("button-primary") : F("button-secondary"));
        sendChunk(F("' href='/admin/email/vorlagen?art=stil'>\xF0\x9F\x8E\xA8 Design</a>"));
        sendChunk(F("</div>"));

        // Rückmeldung des letzten Speicherversuchs
        const String fehler = _server.arg("fehler");
        if (fehler.length()) {
          sendChunk(F("<div class='mail-hinweis' style='color:#ff6f6f'>\xE2\x9A\xA0\xEF\xB8\x8F "));
          if (fehler == "zeile") {
            sendChunk(F("Nicht gespeichert: eine Zeile ist länger als 256 Bytes. Mach zwei "
                        "Absätze daraus."));
          } else if (fehler == "laenge") {
            sendChunk(F("Nicht gespeichert: der Text ist zu lang."));
          } else if (fehler == "betreff") {
            sendChunk(F("Nicht gespeichert: der Betreff ist länger als 120 Bytes."));
          } else if (fehler == "spitz") {
            sendChunk(F("Nicht gespeichert: im Design haben spitze Klammern nichts zu suchen - "
                        "damit ließe sich der Stilblock der Mail verlassen."));
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
        sendChunk(stilSeite ? String(STIL_SCHLUESSEL) : String(art.schluessel));
        sendChunk(F("'>"));

        if (stilSeite) {
          sendeStilFeld(_server);
        } else {
          sendeTextFelder(_server, art);
        }

        sendChunk(F("<div style='margin-top:1em'>"
                    "<button type='submit' class='button-primary'>Speichern</button> "
                    "<button type='submit' name='was' value='reset' class='button-secondary'>"
                    "Auf Standard zurücksetzen</button></div>"));
        sendChunk(F("</form>"));

        sendChunk(F("<div class='mail-hinweis' id='vorlage_befunde'></div>"));

        // Der Stil steckt versteckt in der Seite, damit die Vorschau ihn
        // einsetzen kann, ohne ihn ein zweites Mal vom Gerät zu holen.
        sendChunk(F("<textarea id='vorlage_stil' hidden>"));
        {
          Ausgabe a{&_server, true};
          MailVorlagen::sendeStil(
              [](const char* text, size_t length, void* context) {
                Ausgabe* o = static_cast<Ausgabe*>(context);
                if (!o->ersteZeile) {
                  Component::sendChunk(*o->server, F("\n"));
                }
                o->ersteZeile = false;
                String zeile;
                zeile.reserve(length + 1);
                for (size_t i = 0; i < length; i++) {
                  zeile += text[i];
                }
                Component::sendChunk(*o->server, maskiere(zeile));
              },
              &a);
        }
        sendChunk(F("</textarea>"));

        sendChunk(F("<h3>Vorschau</h3>"));
        sendChunk(F("<iframe id='vorlage_vorschau' sandbox='' "
                    "style='width:100%;height:360px;border:1px solid #444;border-radius:6px;"
                    "background:#fff'></iframe>"));
        sendChunk(F("<div class='mail-hinweis'>Die Vorschau setzt Beispielwerte ein. Die "
                    "Werttabelle richtet sich in der echten Mail nach deinen Sensoren.</div>"));
        sendChunk(F("</div>"));
      },
      css, js);
}

void AdminEmailHandler::handleVorlagenSave() {
  const String artArg = _server.arg("art");
  const bool stilSeite = (artArg == STIL_SCHLUESSEL);
  const Art& art = artVon(artArg);
  const bool zuruecksetzen = (_server.arg("was") == "reset");

  String ziel = String(F("/admin/email/vorlagen?art=")) +
                (stilSeite ? String(STIL_SCHLUESSEL) : String(art.schluessel));
  String fehler;

  if (zuruecksetzen) {
    const bool ok = stilSeite ? MailVorlagen::setzeStilZurueck(fehler)
                              : MailVorlagen::setzeZurueck(art.kind, fehler);
    if (!ok) {
      LOG_ERROR(F("AdminEmail"), String(F("Zuruecksetzen fehlgeschlagen: ")) + fehler);
      ziel += F("&fehler=1");
    }
    sendRedirect(ziel);
    return;
  }

  if (ESP.getFreeHeap() < MIN_HEAP_SPEICHERN) {
    LOG_ERROR(F("AdminEmail"),
              String(F("Zu wenig Speicher zum Speichern: ")) + String(ESP.getFreeHeap()));
    // Aufräumen anstoßen, aber diesem Versuch hilft es nicht mehr: der
    // Handler-Cache enthält genau diesen Handler, deshalb schiebt der
    // WebManager die Arbeit bis nach der Antwort auf. Der nächste Versuch
    // findet dann mehr Platz vor - das ist der Sinn der Meldung "bitte gleich
    // noch einmal versuchen".
    //
    // (Früher stand hier eine Warnung, das auf keinen Fall zu tun: der Aufruf
    // zerstörte den laufenden Handler und endete in "Fatal exception 28". Seit
    // cleanupNonEssentialHandlers() das selbst erkennt, ist er ungefährlich.)
    MemoryMgr.emergencyCleanup();
    sendRedirect(ziel + F("&fehler=speicher"));
    return;
  }

  if (stilSeite) {
    const String cssText = _server.arg("css");
    if (cssText.length() > MailVorlage::STIL_MAX) {
      sendRedirect(ziel + F("&fehler=laenge"));
      return;
    }
    // Eine spitze Klammer im CSS könnte </style> schreiben und damit aus dem
    // Stilblock der Mail ausbrechen. CSS braucht sie nicht - also raus damit,
    // statt später mühsam zu entschärfen.
    if (cssText.indexOf('<') >= 0 || cssText.indexOf('>') >= 0) {
      sendRedirect(ziel + F("&fehler=spitz"));
      return;
    }
    if (!pruefeZeilenlaenge(cssText)) {
      sendRedirect(ziel + F("&fehler=zeile"));
      return;
    }
    if (!MailVorlagen::speichereStil(cssText, fehler)) {
      LOG_ERROR(F("AdminEmail"), String(F("Stil nicht gespeichert: ")) + fehler);
      ziel += F("&fehler=1");
    }
    sendRedirect(ziel);
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

  if (!pruefeZeilenlaenge(rumpfText)) {
    sendRedirect(ziel + F("&fehler=zeile"));
    return;
  }

  if (!MailVorlagen::speichere(art.kind, betreffText, rumpfText, fehler)) {
    LOG_ERROR(F("AdminEmail"), String(F("Vorlage nicht gespeichert: ")) + fehler);
    ziel += F("&fehler=1");
  }
  sendRedirect(ziel);
}
