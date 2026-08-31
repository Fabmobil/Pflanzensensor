/**
 * @file admin_email_handler.cpp
 * @brief Umsetzung der Mail-Konfigurationsseite
 */

#include "web/handler/admin_email_handler.h"

#include <memory>

#include "logger/logger.h"
#include "mail/mail_sender.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/mail_message.h"
#include "web/core/components.h"

extern std::unique_ptr<SensorManager> sensorManager;

namespace {
constexpr uint32_t MIN_HEAP_FOR_PAGE = 6000;

} // namespace

using AdminEmail::maskiere;

bool AdminEmailHandler::ownsUrl(const String& url) {
  return url == F("/admin/email") || url == F("/admin/email/save") ||
         url == F("/admin/email/test") || url == F("/admin/email/status") ||
         url == F("/admin/email/vorlagen") || url == F("/admin/email/vorlagen/save");
}

RouterResult AdminEmailHandler::onRegisterRoutes(WebRouter& router) {
  auto result = router.addRoute(HTTP_GET, "/admin/email", [this]() { handlePage(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_POST, "/admin/email/save", [this]() { handleSave(); });
  if (!result.isSuccess())
    return result;

  // Eigene Route fürs Auslösen: über das Formular ginge es nur zusammen mit
  // dem Speichern, und ein POST, dem ein Feld fehlt, leert die zugehörige
  // Einstellung. Zum Wiederholen eines Versuchs will man nichts ändern.
  result = router.addRoute(HTTP_POST, "/admin/email/test", [this]() { handleTest(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_GET, "/admin/email/status", [this]() { handleStatus(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_GET, "/admin/email/vorlagen", [this]() { handleVorlagen(); });
  if (!result.isSuccess())
    return result;

  result =
      router.addRoute(HTTP_POST, "/admin/email/vorlagen/save", [this]() { handleVorlagenSave(); });
  if (!result.isSuccess())
    return result;

  return RouterResult::success();
}

HandlerResult AdminEmailHandler::handleGet(const String&, const std::map<String, String>&) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Bitte verwenden Sie registerRoutes");
}

HandlerResult AdminEmailHandler::handlePost(const String&, const std::map<String, String>&) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Bitte verwenden Sie registerRoutes");
}

void AdminEmailHandler::sendeSensorAuswahl() {
  if (!sensorManager) {
    sendChunk(F("<em>Keine Sensoren verfügbar</em>"));
    return;
  }
  sendChunk(F("<div class='mail-sensorliste'>"));
  for (const auto& sensor : sensorManager->getSensors()) {
    if (!sensor || !sensor->isEnabled()) {
      continue;
    }
    const SensorConfig& config = sensor->config();
    for (size_t i = 0; i < config.activeMeasurements; i++) {
      if (!config.measurements[i].enabled) {
        continue;
      }
      const String schluessel = sensor->getId() + "_" + String(i);
      String name = config.measurements[i].name;
      if (name.length() == 0) {
        name = sensor->getMeasurementName(i);
      }
      sendChunk(F("<label class='checkbox-label'><input type='checkbox' name='sens' value='"));
      sendChunk(maskiere(schluessel));
      sendChunk(F("'"));
      if (ConfigMgr.isMailSensorWatched(schluessel)) {
        sendChunk(F(" checked"));
      }
      sendChunk(F("> "));
      sendChunk(maskiere(name));
      sendChunk(F("</label>"));
      yield();
    }
  }
  sendChunk(F("</div>"));
}

void AdminEmailHandler::handlePage() {
  if (ESP.getFreeHeap() < MIN_HEAP_FOR_PAGE) {
    _server.send(200, F("text/html"),
                 F("<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><title>E-Mail"
                   "</title></head><body><h1>E-Mail</h1><p>Gerade zu wenig Speicher.</p>"
                   "<p><a href='/admin'>Zurück</a></p></body></html>"));
    return;
  }

  const std::vector<String> css = {};
  const std::vector<String> js = {};

  renderAdminPage(
      ConfigMgr.getDeviceName(), "admin/email",
      [this]() {
        sendChunk(F("<div class='card'>"));
        sendChunk(F("<h2>Mailversand</h2>"));
        sendChunk(F("<form method='post' action='/admin/email/save'>"));

        sendChunk(F("<div><label class='checkbox-label'>"
                    "<input type='checkbox' id='mail_on' name='mail_on' value='1'"));
        if (ConfigMgr.isMailEnabled())
          sendChunk(F(" checked"));
        sendChunk(F("> Mailversand einschalten</label></div>"));

        // Alles Weitere hängt am Schalter oben
        sendChunk(F("<div class='mail-felder' id='mail_felder'>"));

        sendChunk(F("<h3>Zugang</h3>"));
        sendChunk(F("<div><label>SMTP-Server<br><input type='text' name='host' value='"));
        sendChunk(maskiere(ConfigMgr.getMailHost()));
        sendChunk(F("'></label></div>"));

        sendChunk(F("<div><label>Port<br><input type='number' name='port' min='1' max='65535' "
                    "value='"));
        sendChunk(String(ConfigMgr.getMailPort()));
        sendChunk(F("'></label></div>"));
        sendChunk(F("<div class='mail-hinweis'>Port 465 (TLS von Anfang an). Port 587 mit "
                    "STARTTLS wird nicht unterstützt.</div>"));

        sendChunk(F("<div><label>Benutzername<br><input type='text' name='user' value='"));
        sendChunk(maskiere(ConfigMgr.getMailUser()));
        sendChunk(F("'></label></div>"));

        // Das Passwort wird nie zurückgeschickt. Leer lassen heißt: unverändert.
        sendChunk(F("<div><label>Passwort<br><input type='password' name='pwd' placeholder='"));
        sendChunk(ConfigMgr.getMailPassword().length() ? F("gespeichert - leer lassen zum Behalten")
                                                       : F("noch nicht gesetzt"));
        sendChunk(F("'></label></div>"));

        sendChunk(F("<div><label>Absenderadresse<br><input type='text' name='from' value='"));
        sendChunk(maskiere(ConfigMgr.getMailFrom()));
        sendChunk(F("'></label></div>"));

        sendChunk(F("<div><label>Empfängeradresse<br><input type='text' name='to' value='"));
        sendChunk(maskiere(ConfigMgr.getMailTo()));
        sendChunk(F("'></label></div>"));

        sendChunk(F("<h3>Warnmeldungen</h3>"));
        sendChunk(F("<div>Für diese Messwerte warnen:</div>"));
        sendeSensorAuswahl();
        sendChunk(F("<div class='mail-hinweis'>Ohne Auswahl werden alle überwacht.</div>"));

        sendChunk(F("<div><label>Warnen ab <select name='warn_ab'>"));
        sendChunk(F("<option value='1'"));
        if (ConfigMgr.getMailWarnFrom() != 2)
          sendChunk(F(" selected"));
        sendChunk(F(">\xF0\x9F\x9F\xA1 gelb - sobald ein Wert aus dem Wohlfühlbereich "
                    "wandert</option>"));
        sendChunk(F("<option value='2'"));
        if (ConfigMgr.getMailWarnFrom() == 2)
          sendChunk(F(" selected"));
        sendChunk(F(">\xF0\x9F\x94\xB4 rot - erst wenn es der Pflanze wirklich schlecht "
                    "geht</option>"));
        sendChunk(F("</select></label></div>"));

        sendChunk(F("<div><label>Höchstens eine Warnung alle <input type='number' name='warn_h' "
                    "min='1' max='720' style='width:5em' value='"));
        sendChunk(String(ConfigMgr.getMailWarnHours()));
        sendChunk(F("'> Stunden</label></div>"));

        sendChunk(F("<h3>Weitere Meldungen</h3>"));
        sendChunk(F("<div><label class='checkbox-label'><input type='checkbox' name='boot' "
                    "value='1'"));
        if (ConfigMgr.isMailBootEnabled())
          sendChunk(F(" checked"));
        sendChunk(F("> Beim Neustart eine Mail schicken</label></div>"));

        sendChunk(F("<div><label class='checkbox-label'><input type='checkbox' name='alive' "
                    "value='1'"));
        if (ConfigMgr.isMailAliveEnabled())
          sendChunk(F(" checked"));
        sendChunk(F("> Lebenszeichen mit den aktuellen Messwerten</label></div>"));

        sendChunk(F("<div><label>Lebenszeichen alle <input type='number' name='alive_h' min='1' "
                    "max='720' style='width:5em' value='"));
        sendChunk(String(ConfigMgr.getMailAliveHours()));
        sendChunk(F("'> Stunden</label></div>"));

        sendChunk(F("</div>")); // mail-felder

        sendChunk(F("<div style='margin-top:1em'>"
                    "<button type='submit' class='button-primary'>Speichern</button> "
                    "<button type='submit' name='test' value='1' class='button-secondary'>"
                    "Speichern und Testmail senden</button></div>"));
        sendChunk(F("</form>"));

        sendChunk(F("<form method='post' action='/admin/email/test' style='margin-top:0.5em'>"
                    "<button type='submit' class='button-secondary'>Nur Testmail senden "
                    "(ohne Speichern)</button></form>"));

        sendChunk(F("<div style='margin-top:0.8em'><a class='button button-secondary' "
                    "href='/admin/email/vorlagen'>\xE2\x9C\x8F\xEF\xB8\x8F Mailtexte "
                    "bearbeiten</a></div>"));

        // Rückmeldung der letzten Sendung
        sendChunk(F("<div class='mail-status' id='mail_status'>"));
        MailSender& sender = MailSender::instance();
        if (sender.lastResult().length()) {
          sendChunk(F("Letzter Versand: "));
          sendChunk(maskiere(sender.lastResult()));
          sendChunk(F(" ("));
          sendChunk(String(sender.sentCount()));
          sendChunk(F(" erfolgreich)"));
        } else {
          sendChunk(F("Noch nichts verschickt."));
        }
        sendChunk(F("</div>"));

        sendChunk(F("<div class='mail-hinweis'>Verschlüsselung ist Pflicht: das Gerät verbindet "
                    "sich nur über TLS. Die Echtheit des Servers wird dabei nicht geprüft - dafür "
                    "reicht der Speicher dieses Geräts nicht. Der Versand läuft "
                    "im Hintergrund - das Ergebnis erscheint hier nach ein paar Sekunden.</div>"));
        sendChunk(F("</div>"));

        // Kleines Skript statt eigener Datei: eine zusätzliche .js-Datei
        // belegt mit ihrer .gz-Fassung zwei 8-KB-Blöcke im Dateisystem, und
        // das wäre für zwanzig Zeilen unverhältnismäßig.
        sendChunk(
            F("<script>(function(){"
              "var s=document.getElementById('mail_on'),f=document.getElementById('mail_felder');"
              "function u(){f.className='mail-felder'+(s.checked?'':' aus');}"
              "s.addEventListener('change',u);u();"
              "var st=document.getElementById('mail_status');"
              "if(location.search.indexOf('test=1')>=0){var n=0,t=setInterval(function(){"
              "n++;fetch('/admin/email/status').then(function(r){return r.json();})"
              ".then(function(d){if(d.result){st.textContent='Letzter Versand: '+d.result;"
              "if(n>2)clearInterval(t);}});if(n>20)clearInterval(t);},2000);}"
              "})();</script>"));
      },
      css, js);
}

void AdminEmailHandler::handleSave() {
  const bool an = _server.hasArg("mail_on");
  ConfigMgr.setMailEnabled(an);
  ConfigMgr.setMailHost(_server.arg("host"));

  const long port = _server.arg("port").toInt();
  if (port > 0 && port < 65536) {
    ConfigMgr.setMailPort(static_cast<uint16_t>(port));
  }

  ConfigMgr.setMailUser(_server.arg("user"));

  // Leeres Passwortfeld heißt "unverändert" - so muss es beim Ändern anderer
  // Einstellungen nicht jedes Mal neu eingetippt werden, und es steht nie im
  // ausgelieferten HTML.
  const String pwd = _server.arg("pwd");
  if (pwd.length() > 0) {
    ConfigMgr.setMailPassword(pwd);
  }

  ConfigMgr.setMailFrom(_server.arg("from"));
  ConfigMgr.setMailTo(_server.arg("to"));

  // Alle angehakten Sensoren einsammeln. Sind es alle, wird leer gespeichert -
  // dann wirkt die Auswahl auch für später hinzugekommene Messwerte.
  String auswahl;
  const int anzahl = _server.args();
  for (int i = 0; i < anzahl; i++) {
    if (_server.argName(i) == "sens") {
      if (auswahl.length()) {
        auswahl += ',';
      }
      auswahl += _server.arg(i);
    }
  }
  ConfigMgr.setMailSensors(auswahl);

  ConfigMgr.setMailWarnFrom(_server.arg("warn_ab") == "2" ? 2 : 1);

  const long warnH = _server.arg("warn_h").toInt();
  if (warnH > 0) {
    ConfigMgr.setMailWarnHours(static_cast<uint16_t>(warnH));
  }
  const long aliveH = _server.arg("alive_h").toInt();
  if (aliveH > 0) {
    ConfigMgr.setMailAliveHours(static_cast<uint16_t>(aliveH));
  }
  ConfigMgr.setMailBootEnabled(_server.hasArg("boot"));
  ConfigMgr.setMailAliveEnabled(_server.hasArg("alive"));

  MailSender::instance().reloadConfig();

  const bool test = _server.hasArg("test");
  if (test) {
    // Nur vormerken: der Versand dauert Sekunden und braucht rund 10 KB Heap.
    // Aus diesem Handler heraus stünde der Webserver so lange still.
    MailSender::instance().requestTestMail();
    LOG_INFO(F("AdminEmail"), F("Testmail angefordert"));
  }

  sendRedirect(test ? F("/admin/email?test=1") : F("/admin/email"));
}

void AdminEmailHandler::handleTest() {
  MailSender::instance().requestTestMail();
  LOG_INFO(F("AdminEmail"), F("Testmail angefordert (ohne Aenderung der Einstellungen)"));
  sendRedirect(F("/admin/email?test=1"));
}

void AdminEmailHandler::handleStatus() {
  MailSender& sender = MailSender::instance();
  String json = F("{\"pending\":");
  json += sender.testMailPending() ? F("true") : F("false");
  json += F(",\"sent\":");
  json += String(sender.sentCount());
  json += F(",\"result\":\"");
  String text = sender.lastResult();
  text.replace("\"", "'");
  json += text;
  json += F("\"}");
  sendJsonResponse(200, json);
}
