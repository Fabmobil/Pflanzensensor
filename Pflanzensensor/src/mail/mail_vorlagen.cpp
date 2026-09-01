/**
 * @file mail_vorlagen.cpp
 * @brief Umsetzung der Vorlagenverwaltung (siehe mail_vorlagen.h)
 */

#include "mail/mail_vorlagen.h"

#include <LittleFS.h>
#include <memory>

#include "logger/logger.h"
#include "mail/mail_vorlagen_standard.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "utils/helper.h"
#include "utils/mdns_name.h"
#include "utils/wifi.h"
#include "web/core/components.h"

extern std::unique_ptr<SensorManager> sensorManager;

using MailVorlage::Abschnitt;

namespace {

/// Zeilenpuffer. 256 wie beim Ausliefern statischer Dateien - der Stack des
/// ESP8266 ist nur 4 KB groß (web_manager_static.cpp:101).
constexpr size_t ZEILE_PUFFER = MailVorlage::ZEILE_MAX + 2;

const char* standardFuer(Abschnitt a) {
  switch (a) {
  case Abschnitt::BootBetreff:
    return MailVorlagenStandard::BOOT_BETREFF;
  case Abschnitt::BootRumpf:
    return MailVorlagenStandard::BOOT_RUMPF;
  case Abschnitt::WarnBetreff:
    return MailVorlagenStandard::WARNUNG_BETREFF;
  case Abschnitt::WarnRumpf:
    return MailVorlagenStandard::WARNUNG_RUMPF;
  case Abschnitt::AliveBetreff:
    return MailVorlagenStandard::ALIVE_BETREFF;
  case Abschnitt::AliveRumpf:
    return MailVorlagenStandard::ALIVE_RUMPF;
  case Abschnitt::Stil:
    return MailVorlagenStandard::STIL;
  default:
    return nullptr;
  }
}

/// Was mit einer gelesenen Rohzeile geschehen soll. false bricht ab.
using RohZeile = bool (*)(const char* zeile, void* context);

/**
 * @brief Einen Abschnitt aus der Datei zeilenweise liefern
 * @return false wenn die Datei fehlt, fremd ist oder den Abschnitt nicht hat
 * @details Die Zeilen kommen bereits entwertet an (führendes \ entfernt).
 */
bool leseAusDatei(Abschnitt gesucht, RohZeile aus, void* context) {
  if (!LittleFS.exists(MailVorlagen::PFAD)) {
    return false;
  }
  File datei = LittleFS.open(MailVorlagen::PFAD, "r");
  if (!datei) {
    return false;
  }

  char zeile[ZEILE_PUFFER];
  size_t n = datei.readBytesUntil('\n', zeile, sizeof(zeile) - 1);
  zeile[n] = '\0';
  if (strcmp(zeile, MailVorlage::DATEI_KOPF) != 0) {
    // Fremde Datei - lieber den Standard nehmen als Unsinn verschicken.
    LOG_WARN(F("Vorlagen"), F("Vorlagendatei hat eine fremde Kopfzeile - Standard wird benutzt"));
    datei.close();
    return false;
  }

  bool imAbschnitt = false;
  bool etwasGeliefert = false;
  while (datei.available()) {
    n = datei.readBytesUntil('\n', zeile, sizeof(zeile) - 1);

    // Wurde der Puffer voll, ohne dass ein Zeilenende kam, steht der Rest der
    // Zeile noch in der Datei. Dann darf nicht mitten in einem UTF-8-Zeichen
    // geschnitten werden: die angefangenen Bytes wandern zurück in die Datei
    // und kommen beim nächsten Durchlauf mit ihrem Rest zusammen. Ohne das
    // zerfiel ein Emoji in zwei Ersatzzeichen.
    if (n == sizeof(zeile) - 1) {
      const size_t ganz = MailVorlage::ganzeZeichen(zeile, n);
      if (ganz < n) {
        datei.seek(static_cast<int32_t>(ganz) - static_cast<int32_t>(n), SeekCur);
        n = ganz;
      }
    }

    zeile[n] = '\0';
    // readBytesUntil lässt ein \r stehen, wenn die Datei CRLF benutzt
    while (n > 0 && zeile[n - 1] == '\r') {
      zeile[--n] = '\0';
    }

    const Abschnitt marke = MailVorlage::erkenneMarke(zeile);
    if (marke != Abschnitt::Keiner) {
      if (imAbschnitt) {
        break; // nächster Abschnitt: fertig
      }
      imAbschnitt = (marke == gesucht);
      continue;
    }
    if (!imAbschnitt) {
      continue;
    }
    etwasGeliefert = true;
    if (!aus(MailVorlage::entwerte(zeile), context)) {
      break;
    }
    optimistic_yield(1000);
  }

  datei.close();
  return imAbschnitt || etwasGeliefert;
}

/// Denselben Abschnitt aus dem PROGMEM-Standard liefern.
void leseAusStandard(Abschnitt gesucht, RohZeile aus, void* context) {
  const char* quelle = standardFuer(gesucht);
  if (!quelle) {
    return;
  }
  char zeile[ZEILE_PUFFER];
  size_t at = 0;
  for (size_t i = 0;; i++) {
    const char c = static_cast<char>(pgm_read_byte(quelle + i));
    if (c == '\0' || c == '\n') {
      zeile[at] = '\0';
      if (at > 0 || c == '\n') {
        if (!aus(zeile, context)) {
          return;
        }
      }
      at = 0;
      if (c == '\0') {
        return;
      }
      continue;
    }
    if (at < sizeof(zeile) - 1) {
      zeile[at++] = c;
    }
  }
}

/// Erst die Datei, sonst der Standard.
void leseAbschnitt(Abschnitt gesucht, RohZeile aus, void* context) {
  if (!leseAusDatei(gesucht, aus, context)) {
    leseAusStandard(gesucht, aus, context);
  }
}

// === Werte für die Platzhalter ===

/**
 * @brief Sammelt die Werte einmal ein
 * @details Als Liste und nicht als Rückruf, weil Helper::getRebootCount() bei
 *          jedem Aufruf /reboot_count.txt vom Flash liest (helper.cpp:51-64) -
 *          bei zwei Vorkommen im Text wäre das zweimal Flash-Zugriff mitten in
 *          einer offenen TLS-Verbindung.
 */
struct Werte {
  String geraet, name, ip, ssid, neustarts, laufzeit, datum, uhrzeit;
  MailVorlage::Paar paare[8];

  Werte() {
    geraet = ConfigMgr.getDeviceName();
    // Der Name, unter dem das Gerät im Netz erreichbar ist. Er überlebt einen
    // Wechsel der IP - genau deshalb steht er in der Mail.
    name = mdnsName();
    if (name.length() == 0) {
      char host[MdnsName::MAX_LEN + 1];
      MdnsName::hostnameVon(geraet.c_str(), host, sizeof(host));
      name = host;
    }
    name += F(".local");
    ip = Component::getDisplayIP();
    ssid = Component::getDisplaySSID();
    neustarts = String(Helper::getRebootCount());
    laufzeit = Helper::getFormattedUptime();
    datum = Helper::getFormattedDate();
    uhrzeit = Helper::getFormattedTime(true);

    paare[0] = {"geraet", geraet.c_str()};
    paare[1] = {"name", name.c_str()};
    paare[2] = {"ip", ip.c_str()};
    paare[3] = {"ssid", ssid.c_str()};
    paare[4] = {"neustarts", neustarts.c_str()};
    paare[5] = {"laufzeit", laufzeit.c_str()};
    paare[6] = {"datum", datum.c_str()};
    paare[7] = {"uhrzeit", uhrzeit.c_str()};
  }
};

/// Ampelzeichen zum Status, wie ihn Sensor::getStatus() liefert.
const char* ampel(const String& status) {
  if (status == F("green"))
    return "\xF0\x9F\x9F\xA2"; // 🟢
  if (status == F("yellow"))
    return "\xF0\x9F\x9F\xA1"; // 🟡
  if (status == F("red"))
    return "\xF0\x9F\x94\xB4"; // 🔴
  return "\xE2\x9A\xAA";       // ⚪
}

/**
 * @brief Erzeugt die Werttabelle für {messwerte} und {auffaellige}
 * @details Erzeugt die vollständige Tabelle samt <table>, nicht nur die Zeilen:
 *          im Vorlagentext steht kein HTML mehr, das der Nutzer aufmachen und
 *          wieder schließen könnte. Gestaltet wird über die Klassen im Stil -
 *          table.werte, td.name, td.wert.
 */
void gibMesswerte(const char* name, MailVorlage::ZeilenSenke aus, void* senkeContext, void*) {
  const bool nurAuffaellige = (strcmp(name, MailVorlage::BLOCK_AUFFAELLIGE) == 0);
  if (!sensorManager) {
    return;
  }

  char zeile[MailVorlage::ZEILE_MAX + 1];
  bool etwasGezeigt = false;

  const char* auf = "<table class=\"werte\">";
  aus(auf, strlen(auf), senkeContext);

  for (const auto& sensor : sensorManager->getSensors()) {
    if (!sensor || !sensor->isEnabled()) {
      continue;
    }
    const SensorConfig& config = sensor->config();
    const MeasurementData& daten = sensor->getMeasurementData();
    for (size_t i = 0; i < config.activeMeasurements; i++) {
      if (!config.measurements[i].enabled) {
        continue;
      }
      const String status = sensor->getStatus(i);
      // Dieselbe Regel wie beim Auslösen der Warnmail. Zwei Regeln wären eine
      // Warnung, in der nichts Auffälliges steht.
      const bool auffaellig = Mail::istAuffaellig(
          Mail::levelVonText(status.c_str()),
          ConfigMgr.getMailWarnFrom() == 2 ? Mail::Level::Red : Mail::Level::Yellow);
      const bool ueberwacht = ConfigMgr.isMailSensorWatched(sensor->getId() + "_" + String(i));
      if (nurAuffaellige && (!auffaellig || !ueberwacht)) {
        continue;
      }

      String bezeichnung = config.measurements[i].name;
      if (bezeichnung.length() == 0) {
        bezeichnung = sensor->getMeasurementName(i);
      }
      String wert = F("--");
      if (i < daten.activeValues && !isnan(daten.values[i])) {
        wert = String(daten.values[i], 1) + config.measurements[i].unit;
      }

      snprintf(zeile, sizeof(zeile),
               "<tr><td class=\"name\">%s %s</td><td class=\"wert\">%s</td></tr>", ampel(status),
               bezeichnung.c_str(), wert.c_str());
      aus(zeile, strlen(zeile), senkeContext);
      etwasGezeigt = true;
      optimistic_yield(1000);
    }
  }

  if (nurAuffaellige && !etwasGezeigt) {
    // Eine leere Tabelle in der Mail sähe nach einem Fehler aus.
    const char* entwarnung = "<tr><td class=\"name\" colspan=\"2\">"
                             "\xE2\x9C\x85 Jetzt ist wieder alles im grünen Bereich!</td></tr>";
    aus(entwarnung, strlen(entwarnung), senkeContext);
  }

  const char* zu = "</table>";
  aus(zu, strlen(zu), senkeContext);
}

/// Weiterreichen an die eigentliche Senke, für leseAbschnitt().
struct Expander {
  MailVorlage::Umgebung* u;
  MailVorlage::ZeilenSenke aus;
  void* context;
};

bool expandiereUndGibAus(const char* zeile, void* context) {
  Expander* e = static_cast<Expander*>(context);
  MailVorlage::expandiereZeile(zeile, *e->u, e->aus, e->context);
  return true;
}

} // namespace

Abschnitt MailVorlagen::abschnittFuer(Mail::Kind kind, bool istBetreff) {
  switch (kind) {
  case Mail::Kind::Boot:
    return istBetreff ? Abschnitt::BootBetreff : Abschnitt::BootRumpf;
  case Mail::Kind::Warning:
    return istBetreff ? Abschnitt::WarnBetreff : Abschnitt::WarnRumpf;
  default:
    return istBetreff ? Abschnitt::AliveBetreff : Abschnitt::AliveRumpf;
  }
}

size_t MailVorlagen::betreff(Mail::Kind kind, char* out, size_t outSize) {
  Werte werte;
  MailVorlage::Umgebung u;
  u.werte = werte.paare;
  u.anzahl = sizeof(werte.paare) / sizeof(werte.paare[0]);
  // Blöcke bleiben im Betreff bewusst abgeschaltet - siehe expandiereBetreff()

  struct Ziel {
    char* out;
    size_t outSize;
    MailVorlage::Umgebung* u;
    bool fertig;
  } ziel{out, outSize, &u, false};

  auto ersteZeile = [](const char* zeile, void* context) -> bool {
    Ziel* z = static_cast<Ziel*>(context);
    if (z->fertig) {
      return false;
    }
    MailVorlage::expandiereBetreff(zeile, *z->u, z->out, z->outSize);
    z->fertig = true;
    return false; // weitere Zeilen im Betreffabschnitt interessieren nicht
  };

  out[0] = '\0';
  leseAbschnitt(abschnittFuer(kind, true), ersteZeile, &ziel);
  return strlen(out);
}

void MailVorlagen::sendeRumpf(Mail::Kind kind, MailVorlage::ZeilenSenke aus, void* context) {
  const uint32_t t0 = millis();
  Werte werte;
  MailVorlage::Umgebung u;
  u.werte = werte.paare;
  u.anzahl = sizeof(werte.paare) / sizeof(werte.paare[0]);
  u.bloecke = gibMesswerte;
  u.blockContext = nullptr;

  Expander e{&u, aus, context};
  leseAbschnitt(abschnittFuer(kind, false), expandiereUndGibAus, &e);
  if (ConfigMgr.isDebugMail()) {
    LOG_DEBUG(F("Vorlagen"), String(F("Rumpf gestreamt in ")) + String(millis() - t0) + F(" ms"));
  }
}

void MailVorlagen::sendeRoh(Abschnitt abschnitt, RohSenke aus, void* context) {
  struct Ziel {
    RohSenke aus;
    void* context;
  } ziel{aus, context};

  auto weiter = [](const char* zeile, void* c) -> bool {
    Ziel* z = static_cast<Ziel*>(c);
    z->aus(zeile, z->context);
    return true;
  };
  leseAbschnitt(abschnitt, weiter, &ziel);
}

bool MailVorlagen::istAngepasst(Mail::Kind kind) {
  auto nichts = [](const char*, void*) -> bool { return false; };
  return leseAusDatei(abschnittFuer(kind, false), nichts, nullptr);
}

namespace {

/// Eine Zeile entwertet in die Datei schreiben.
void schreibeZeile(File& datei, const char* zeile) {
  if (MailVorlage::brauchtEntwertung(zeile)) {
    datei.print('\\');
  }
  datei.print(zeile);
  datei.print('\n');
}

/// Text zeilenweise schreiben, CRLF zu LF normalisieren.
void schreibeText(File& datei, const String& text) {
  int start = 0;
  while (start <= static_cast<int>(text.length())) {
    int ende = text.indexOf('\n', start);
    String zeile = (ende < 0) ? text.substring(start) : text.substring(start, ende);
    while (zeile.length() && zeile[zeile.length() - 1] == '\r') {
      zeile.remove(zeile.length() - 1);
    }
    schreibeZeile(datei, zeile.c_str());
    if (ende < 0) {
      break;
    }
    start = ende + 1;
    yield();
  }
}

} // namespace

namespace {

/// Ein Abschnitt, der neu geschrieben werden soll. text == nullptr heißt:
/// entfernen, damit beim Lesen wieder der Standard greift.
struct Neuschrift {
  Abschnitt abschnitt;
  const String* text;
  bool eineZeile; ///< Betreff: Umbrüche werden zu Leerzeichen
};

/**
 * @brief Datei neu schreiben: die genannten Abschnitte ersetzen, den Rest
 *        unverändert übernehmen
 * @details Ein einziger Durchlauf über die alte Datei, atomar über eine
 *          Zwischendatei und rename() - dasselbe Muster wie saveJsonFile() in
 *          utils/json_file_utils.h. Speichern und Zurücksetzen sind derselbe
 *          Vorgang; der Unterschied ist nur, ob ein Text mitkommt.
 */
bool schreibeDateiNeu(const Neuschrift* neue, size_t anzahl, String& fehler) {
  const String neuerPfad = String(MailVorlagen::PFAD) + ".neu";
  File aus = LittleFS.open(neuerPfad, "w");
  if (!aus) {
    fehler = F("Vorlagendatei nicht anlegbar");
    return false;
  }
  aus.print(MailVorlage::DATEI_KOPF);
  aus.print('\n');

  // Die neuen Abschnitte zuerst, danach die übrigen aus der alten Datei.
  for (size_t i = 0; i < anzahl; i++) {
    if (!neue[i].text) {
      continue;
    }
    aus.print(MailVorlage::markeVon(neue[i].abschnitt));
    aus.print('\n');
    if (neue[i].eineZeile) {
      String eine = *neue[i].text;
      eine.replace("\r", "");
      eine.replace("\n", " ");
      schreibeZeile(aus, eine.c_str());
    } else {
      schreibeText(aus, *neue[i].text);
    }
  }

  if (LittleFS.exists(MailVorlagen::PFAD)) {
    File alt = LittleFS.open(MailVorlagen::PFAD, "r");
    if (alt) {
      char zeile[ZEILE_PUFFER];
      size_t n = alt.readBytesUntil('\n', zeile, sizeof(zeile) - 1);
      zeile[n] = '\0';
      const bool kopfOk = (strcmp(zeile, MailVorlage::DATEI_KOPF) == 0);

      bool uebernehmen = false;
      while (kopfOk && alt.available()) {
        n = alt.readBytesUntil('\n', zeile, sizeof(zeile) - 1);
        zeile[n] = '\0';
        while (n > 0 && zeile[n - 1] == '\r') {
          zeile[--n] = '\0';
        }

        const Abschnitt marke = MailVorlage::erkenneMarke(zeile);
        if (marke != Abschnitt::Keiner) {
          uebernehmen = (marke != Abschnitt::Unbekannt);
          for (size_t i = 0; i < anzahl && uebernehmen; i++) {
            if (marke == neue[i].abschnitt) {
              uebernehmen = false;
            }
          }
          if (uebernehmen) {
            aus.print(MailVorlage::markeVon(marke));
            aus.print('\n');
          }
          continue;
        }
        if (uebernehmen) {
          // Unverändert übernehmen: die Zeile ist in der alten Datei schon
          // entwertet, sie darf nicht ein zweites Mal entwertet werden.
          aus.print(zeile);
          aus.print('\n');
        }
        optimistic_yield(1000);
      }
      alt.close();
    }
  }

  aus.close();

  // Erst löschen, dann umbenennen - dasselbe Muster wie saveJsonFile()
  LittleFS.remove(MailVorlagen::PFAD);
  if (!LittleFS.rename(neuerPfad, MailVorlagen::PFAD)) {
    fehler = F("Vorlagendatei konnte nicht ersetzt werden");
    LittleFS.remove(neuerPfad);
    return false;
  }
  return true;
}

} // namespace

bool MailVorlagen::speichere(Mail::Kind kind, const String& betreffText, const String& rumpfText,
                             String& fehler) {
  const Neuschrift neue[] = {{abschnittFuer(kind, true), &betreffText, true},
                             {abschnittFuer(kind, false), &rumpfText, false}};
  if (!schreibeDateiNeu(neue, 2, fehler)) {
    return false;
  }
  LOG_INFO(F("Vorlagen"), F("Vorlage gespeichert"));
  return true;
}

bool MailVorlagen::speichereStil(const String& css, String& fehler) {
  const Neuschrift neue[] = {{Abschnitt::Stil, &css, false}};
  if (!schreibeDateiNeu(neue, 1, fehler)) {
    return false;
  }
  LOG_INFO(F("Vorlagen"), F("Stil gespeichert"));
  return true;
}

bool MailVorlagen::setzeZurueck(Mail::Kind kind, String& fehler) {
  // Zurücksetzen heißt: den Abschnitt aus der Datei entfernen. Beim Lesen
  // greift dann wieder der Standard aus dem PROGMEM.
  if (!LittleFS.exists(PFAD)) {
    return true; // schon Standard
  }
  const Neuschrift weg[] = {{abschnittFuer(kind, true), nullptr, true},
                            {abschnittFuer(kind, false), nullptr, false}};
  if (!schreibeDateiNeu(weg, 2, fehler)) {
    return false;
  }
  LOG_INFO(F("Vorlagen"), F("Vorlage auf Standard zurückgesetzt"));
  return true;
}

bool MailVorlagen::setzeStilZurueck(String& fehler) {
  if (!LittleFS.exists(PFAD)) {
    return true;
  }
  const Neuschrift weg[] = {{Abschnitt::Stil, nullptr, false}};
  if (!schreibeDateiNeu(weg, 1, fehler)) {
    return false;
  }
  LOG_INFO(F("Vorlagen"), F("Stil auf Standard zurückgesetzt"));
  return true;
}

void MailVorlagen::sendeStil(MailVorlage::ZeilenSenke aus, void* context) {
  struct Ziel {
    MailVorlage::ZeilenSenke aus;
    void* context;
  } ziel{aus, context};

  leseAbschnitt(
      Abschnitt::Stil,
      [](const char* zeile, void* zielZeiger) {
        Ziel* z = static_cast<Ziel*>(zielZeiger);
        z->aus(zeile, strlen(zeile), z->context);
        return true;
      },
      &ziel);
}
