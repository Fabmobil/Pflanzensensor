/**
 * @file chronik_handler.cpp
 * @brief Umsetzung der Chronik-Seite und ihrer Datenschnittstelle
 */

#include "web/handler/chronik_handler.h"

#include <memory>

#include "chronik/chronik_recorder.h"
#include "chronik/chronik_store.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/chronik_format.h"
#include "web/core/components.h"

using namespace ChronikFormat;

namespace {

/// Unterhalb dieser Grenze wird gar nicht erst begonnen. Wie beim LogHandler
/// höher angesetzt als der Standardwächter von 4096: der Seitenaufbau
/// allokiert unterwegs noch.
constexpr uint32_t MIN_HEAP_FOR_PAGE = 6000;

/// Sink für den Rohdatenstrom: reicht die Bytes unverändert an den Browser
/// weiter und bricht ab, wenn der Heap knapp wird.
bool sendeRoh(void* context, const uint8_t* data, size_t length) {
  ESPWebServer* server = static_cast<ESPWebServer*>(context);
  if (ESP.getFreeHeap() < 4096) {
    LOG_WARN(F("Chronik"), F("Heap knapp - Datenstrom wird abgebrochen"));
    return false;
  }
  server->sendContent(reinterpret_cast<const char*>(data), length);
  return true;
}

/**
 * @brief Zustand des CSV-Exports
 * @details Der Rohstrom kommt in 256-Byte-Häppchen, Rahmen liegen aber quer
 *          über die Häppchengrenzen. Deshalb ein Fenster, aus dem vollständige
 *          Rahmen herausgelöst werden; der Rest wandert nach vorn und wartet
 *          auf das nächste Häppchen.
 *
 *          Bewusst auf dem Heap angelegt: gut ein Kilobyte auf einem Stack von
 *          vier hätte hier nichts verloren.
 */
struct CsvContext {
  ESPWebServer* server{nullptr};
  uint8_t window[MAX_FRAME_SIZE + 256]{};
  size_t length{0};
  /// Kanalschlüssel aus der zuletzt gelesenen Tabelle, damit die CSV-Zeilen
  /// "ANALOG_0" statt einer nackten Nummer tragen.
  char keys[MAX_CHANNELS][MAX_TEXT + 1]{};
  String out;
  bool aborted{false};
};

void merkeSchluessel(void* context, const TableEntry& entry) {
  CsvContext* ctx = static_cast<CsvContext*>(context);
  if (entry.channel >= MAX_CHANNELS) {
    return;
  }
  memcpy(ctx->keys[entry.channel], entry.key, entry.keyLength);
  ctx->keys[entry.channel][entry.keyLength] = '\0';
}

const char* statusText(uint8_t status) {
  switch (status) {
  case STATUS_GREEN:
    return "green";
  case STATUS_YELLOW:
    return "yellow";
  case STATUS_RED:
    return "red";
  case STATUS_ERROR:
    return "error";
  case STATUS_WARMUP:
    return "warmup";
  default:
    return "unknown";
  }
}

/// Vollständige Rahmen aus dem Fenster nach CSV umsetzen.
void verarbeiteFenster(CsvContext* ctx) {
  size_t at = 0;
  while (at < ctx->length) {
    const size_t rest = ctx->length - at;
    SampleFrame frame;
    size_t consumed = readSample(ctx->window + at, rest, frame);
    if (consumed > 0) {
      for (uint8_t i = 0; i < frame.count; i++) {
        const ChannelValue& value = frame.values[i];
        ctx->out += String(frame.epoch);
        ctx->out += ',';
        ctx->out += String(value.channel);
        ctx->out += ',';
        ctx->out += ctx->keys[value.channel];
        ctx->out += ',';
        ctx->out += String(value.value, 3);
        ctx->out += ',';
        ctx->out += value.hasRaw ? String(value.raw) : String();
        ctx->out += ',';
        ctx->out += statusText(value.status);
        ctx->out += '\n';
      }
      at += consumed;
      continue;
    }

    consumed = readTable(ctx->window + at, rest, merkeSchluessel, ctx);
    if (consumed > 0) {
      at += consumed;
      continue;
    }

    // Kein vollständiger Rahmen: entweder fehlen noch Bytes (dann warten wir)
    // oder die Stelle ist beschädigt (dann setzen wir auf dem nächsten Magic
    // wieder auf, statt den Rest zu verlieren).
    if (rest < MAX_FRAME_SIZE) {
      break;
    }
    const size_t naechstes = findMagic(ctx->window + at, rest, 1);
    at += (naechstes >= rest) ? rest : naechstes;
  }

  if (at > 0) {
    memmove(ctx->window, ctx->window + at, ctx->length - at);
    ctx->length -= at;
  }

  if (ctx->out.length() >= 512) {
    ctx->server->sendContent(ctx->out);
    ctx->out = "";
  }
}

bool sendeCsv(void* context, const uint8_t* data, size_t length) {
  CsvContext* ctx = static_cast<CsvContext*>(context);
  if (ESP.getFreeHeap() < 4096) {
    ctx->aborted = true;
    return false;
  }

  size_t offset = 0;
  while (offset < length) {
    const size_t platz = sizeof(ctx->window) - ctx->length;
    const size_t nimm = (length - offset) < platz ? (length - offset) : platz;
    memcpy(ctx->window + ctx->length, data + offset, nimm);
    ctx->length += nimm;
    offset += nimm;
    verarbeiteFenster(ctx);
    if (nimm == 0) {
      // Fenster voll und nichts verarbeitbar - kann nur beschädigt sein.
      ctx->length = 0;
      break;
    }
    optimistic_yield(1000);
  }
  return true;
}

} // namespace

bool ChronikHandler::ownsUrl(const String& url) {
  return url == F("/chronik") || url == F("/chronik/daten") || url == F("/chronik/export.csv");
}

RouterResult ChronikHandler::onRegisterRoutes(WebRouter& router) {
  LOG_DEBUG(F("Chronik"), F("Registriere Chronik-Routen"));

  auto result = router.addRoute(HTTP_GET, "/chronik", [this]() { handlePage(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_GET, "/chronik/daten", [this]() { handleData(); });
  if (!result.isSuccess())
    return result;

  result = router.addRoute(HTTP_GET, "/chronik/export.csv", [this]() { handleExport(); });
  if (!result.isSuccess())
    return result;

  return RouterResult::success();
}

HandlerResult ChronikHandler::handleGet(const String&, const std::map<String, String>&) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Bitte verwenden Sie registerRoutes");
}

HandlerResult ChronikHandler::handlePost(const String&, const std::map<String, String>&) {
  return HandlerResult::fail(HandlerError::INVALID_REQUEST, "Bitte verwenden Sie registerRoutes");
}

void ChronikHandler::handlePage() {
  if (ESP.getFreeHeap() < MIN_HEAP_FOR_PAGE) {
    LOG_WARN(F("Chronik"), F("Wenig Speicher - liefere Kurzfassung"));
    _server.send(200, F("text/html"),
                 F("<!DOCTYPE html><html lang='de'><head><meta charset='UTF-8'><title>Chronik"
                   "</title></head><body><h1>Chronik</h1><p>Gerade zu wenig Speicher. Bitte "
                   "gleich noch einmal versuchen.</p><p><a href='/'>Zur Startseite</a></p>"
                   "</body></html>"));
    return;
  }

  const std::vector<String> css = {};
  const std::vector<String> js = {"chronik"};

  ChronikStore& store = ChronikStore::instance();

  renderAdminPage(
      ConfigMgr.getDeviceName(), "chronik",
      [this, &store]() {
        sendChunk(F("<div class='card chronik-card'>"));

        // Bedienleiste: Zeitbereich, Rohwerte, Nachladen, Export
        sendChunk(F("<div class='chronik-controls'>"));
        sendChunk(F("<div class='chronik-ranges' id='chronik-ranges'>"));
        sendChunk(
            F("<button type='button' class='button-secondary' data-range='3600'>1 h</button>"));
        sendChunk(
            F("<button type='button' class='button-secondary' data-range='21600'>6 h</button>"));
        sendChunk(
            F("<button type='button' class='button-primary' data-range='86400'>24 h</button>"));
        sendChunk(
            F("<button type='button' class='button-secondary' data-range='0'>Alles</button>"));
        sendChunk(F("</div>"));
        sendChunk(F("<a class='button button-secondary' href='/chronik/export.csv'>CSV</a>"));
        sendChunk(F("</div>"));

        sendChunk(F("<div class='chronik-canvas-wrap'>"));
        sendChunk(F("<canvas id='chronik-canvas'></canvas>"));
        sendChunk(F("<div class='chronik-hint' id='chronik-hint'>Daten werden geladen …</div>"));
        sendChunk(F("</div>"));

        sendChunk(F("<div class='chronik-legend' id='chronik-legend'></div>"));
        sendChunk(F("<div class='chronik-note'>Ziehen verschiebt, Mausrad zoomt. Ein Klick in "
                    "der Legende blendet eine Reihe ein oder aus; die Rohwerte der Analogkanäle "
                    "stehen dort als eigene Einträge und sind anfangs ausgeblendet. Ein Rohwert "
                    "ist das letzte Einzelsample, der Messwert daneben das Mittel aus "
                    "mehreren. Bleibt genau eine Messreihe übrig, zeigt das Diagramm ihre "
                    "Schwellwerte als Linien und den grünen Bereich als Fläche.</div>"));
        sendChunk(F("</div>"));

        // Speicherzustand - macht das rollende Fenster nachvollziehbar
        sendChunk(F("<div class='card chronik-card'>"));
        sendChunk(F("<table class='info-table'>"));
        sendChunk(F("<tr><td>Segmente:</td><td>"));
        sendChunk(String(store.segmentCount()));
        sendChunk(F(" von "));
        sendChunk(String(store.targetSegments()));
        sendChunk(F("</td></tr><tr><td>Belegt:</td><td>"));
        // In Bytes, nicht in Kilobyte: im ersten Segment stünde sonst tagelang
        // "0 KB" und niemand wüsste, ob überhaupt etwas aufgezeichnet wird.
        sendChunk(String(store.usedBytes()));
        sendChunk(F(" B von "));
        sendChunk(String(static_cast<uint32_t>(store.targetSegments()) * 7936 / 1024));
        sendChunk(F(" KB</td></tr><tr><td>Aufzeichnung:</td><td>"));
        if (store.isEnabled()) {
          sendChunk(F("aktiv"));
        } else if (store.isDisabledForBoot()) {
          // Nach mehreren schnellen Neustarts hintereinander - etwa direkt nach
          // einem Update - hält sich die Chronik einen Start lang zurück.
          sendChunk(F("bis zum nächsten Neustart aus (Mehrfachstart erkannt)"));
        } else {
          sendChunk(F("aus (zu wenig Platz)"));
        }
        sendChunk(F("</td></tr><tr><td>Verworfen:</td><td>"));
        sendChunk(String(store.droppedFrames()));
        sendChunk(F("</td></tr>"));
        if (ChronikRecorder::skippedChannels() > 0) {
          sendChunk(F("<tr><td>Nicht aufgezeichnet:</td><td>"));
          sendChunk(String(ChronikRecorder::skippedChannels()));
          sendChunk(F(" Kanäle über der Grenze von 16</td></tr>"));
        }
        sendChunk(F("</table>"));
        sendChunk(F("<div class='chronik-note'>Ein Dateisystem-Update löscht die Chronik. Wer "
                    "sie behalten will, lädt vorher die CSV-Datei herunter.</div>"));
        sendChunk(F("</div>"));
      },
      css, js);
}

void ChronikHandler::handleData() {
  if (ESP.getFreeHeap() < MIN_HEAP_FOR_PAGE) {
    _server.send(503, F("application/octet-stream"), F(""));
    return;
  }

  uint32_t since = 0;
  if (_server.hasArg("seit")) {
    since = static_cast<uint32_t>(strtoul(_server.arg("seit").c_str(), nullptr, 10));
  }

  // Keine Content-Length: die müsste erst über alle Segmente gezählt werden.
  beginChunkedResponse(F("application/octet-stream"));
  ChronikStore::instance().streamFrom(since, sendeRoh, &_server);
  endChunkedResponse();
}

void ChronikHandler::handleExport() {
  // Siehe EXPORT_MIN_INTERVAL_MS: funktionslokal statisch, damit die Sperre die
  // Verdrängung des Handlers aus dem LRU-Cache überlebt.
  static RequestThrottle throttle(EXPORT_MIN_INTERVAL_MS);

  if (!throttle.tryAcquire(millis())) {
    _server.sendHeader(F("Retry-After"), F("10"));
    _server.send(429, F("text/plain"), F("Bitte kurz warten - der Export läuft schon.\n"));
    return;
  }

  if (ESP.getFreeHeap() < MIN_HEAP_FOR_PAGE) {
    _server.send(503, F("text/plain"), F("Gerade zu wenig Speicher.\n"));
    return;
  }

  std::unique_ptr<CsvContext> ctx(new CsvContext());
  if (!ctx) {
    _server.send(503, F("text/plain"), F("Gerade zu wenig Speicher.\n"));
    return;
  }
  ctx->server = &_server;
  ctx->out.reserve(640);

  _server.sendHeader(F("Content-Disposition"), F("attachment; filename=chronik.csv"));
  beginChunkedResponse(F("text/csv"));
  sendChunk(F("zeit,kanal,schluessel,wert,rohwert,status\n"));

  ChronikStore::instance().streamFrom(0, sendeCsv, ctx.get());

  // Was nach dem letzten Häppchen noch im Fenster steht, ist ein Restrahmen
  // ohne Nachschub - einmal versuchen, dann den Puffer leeren.
  verarbeiteFenster(ctx.get());
  if (ctx->out.length() > 0) {
    _server.sendContent(ctx->out);
  }
  if (ctx->aborted) {
    sendChunk(F("# Abbruch: zu wenig Speicher\n"));
  }
  endChunkedResponse();
}
