/**
 * @file web_manager_init.cpp
 * @brief WebManager-Initialisierung und Dienst-Einrichtung
 */

#include "web/core/web_auth.h"
#include <stdexcept>

#include "configs/config.h"
#include "logger/logger.h"
#include "web/core/web_manager.h"
#include "web/handler/log_handler.h"
#if USE_WEBSOCKET
#include "web/services/websocket.h"
#endif
#if USE_PROMETHEUS_METRICS
#include "web/handler/web_metrics_handler.h"
#endif
#include "utils/memory_manager.h"
#include "utils/wifi.h"

ResourceResult WebManager::begin(uint16_t port) {
  if (_initialized) {
    LOG_WARN(F("WebManager"), F("WebManager bereits initialisiert"));
    return ResourceResult::success();
  }

  _port = port;
  LOG_INFO(F("WebManager"), "Initialisiere WebManager auf Port " + String(_port));

  // Wichtige Dienste zuerst initialisieren
  _server = std::make_unique<ESPWebServer>(_port);
  // ESP8266WebServer hebt nur ausdrücklich angeforderte Kopfzeilen auf. Ohne
  // das hier liefert server.header("Accept-Encoding") immer eine leere
  // Zeichenkette, und tryServeStaticFile() könnte nicht erkennen, ob der
  // Browser gzip verträgt.
  collectStaticHeaders();
  _auth = std::make_unique<WebAuth>(*_server);
  _router = std::make_unique<WebRouter>(*_server);
  _cssService = std::make_unique<CSSService>(*_server);
  _otaHandler = std::make_unique<WebOTAHandler>(*_server, *_auth);
#if USE_PROMETHEUS_METRICS
  _metricsHandler = std::make_unique<WebMetricsHandler>();
  _metricsHandler->init(); // Zustand auf INITIALIZED setzen, damit /metrics aktiv ist
  // SensorManager-Referenz weiterleiten, falls bereits gesetzt
  if (_sensorManager) {
    _metricsHandler->setSensorManager(*_sensorManager);
  }
#endif

#if USE_WEBSOCKET
  // WebSocket-Server zuerst initialisieren
  if (!WebSocketService::getInstance().init(81, nullptr)) {
    LOG_ERROR(F("WebManager"), F("WebSocket-Server konnte nicht initialisiert werden"));
    return ResourceResult::fail(ResourceError::WEBSOCKET_ERROR,
                                F("WebSocket-Server konnte nicht initialisiert werden"));
  }

  // Event-Handler für WebSocket setzen - LogHandler wird dynamisch aus Cache geholt
  auto& ws = WebSocketService::getInstance();
  ws.setEventHandler([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    // LogHandler aus Cache holen (wird lazy-geladen wenn nötig)
    auto* logHandler = getCachedHandler("log");
    if (logHandler) {
      static_cast<LogHandler*>(logHandler)->handleWebSocketEvent(num, type, payload, length);
    }
  });
#endif

  // Aufräumroutine für Speichernot registrieren: der Handler-Cache ist der
  // mit Abstand größte Block, der sich gefahrlos freigeben lässt (die Handler
  // werden bei Bedarf ohnehin neu geladen).
  MemoryMgr.setCleanupHandler([]() { WebManager::getInstance().cleanupNonEssentialHandlers(); });

  // Middleware und Basisrouten einrichten
  setupMiddleware();

  // Webdienste (inkl. statischer Dateien) initialisieren
  setupServices();

  // Routen einrichten (Handler werden lazy-geladen)
  setupRoutes();

  // Lazy-Loading-Middleware sofort registrieren
  initializeRemainingHandlers();

  _server->begin();
  _initialized = true;

  return ResourceResult::success();
}

ResourceResult WebManager::beginUpdateMode() {
  LOG_INFO(F("WebManager"), F("Wechsel in minimalen Update-Modus"));

  // Startzeit für Update-Modus setzen (Timeout-Absicherung)
  m_updateModeStartTime = millis();
  LOG_DEBUG(F("WebManager"),
            String(F("Update-Modus Startzeit gesetzt: ")) + String(m_updateModeStartTime));

  // Alle Dienste zuerst stoppen
  if (_sensorManager) {
    LOG_INFO(F("WebManager"), F("Sensor-Manager wird gestoppt"));
    _sensorManager->stopAll();
    _sensorManager = nullptr;
  }

  // Speicher freigeben vor Update
  stop();
  cleanup();

  delay(500);
#ifndef ESP32
  ESP.wdtFeed();
#endif

  // Minimale Dienste mit expliziten Speicherzuweisungen erstellen
  logger.logMemoryStats(F("vor_minimalen_diensten"));
  auto setupResult = setupMinimalServices();
  if (!setupResult.isSuccess()) {
    LOG_ERROR(F("WebManager"), String(F("Minimale Dienste konnten nicht eingerichtet werden: ")) +
                                   setupResult.getMessage());
    return ResourceResult::fail(ResourceError::WEBSERVER_ERROR,
                                String(F("Minimale Dienste konnten nicht eingerichtet werden: ")) +
                                    setupResult.getMessage());
  }

  // Explizit als Minimalmodus markieren
  m_handlersInitialized = true; // Volle Handler-Initialisierung verhindern

  // Nur Minimalmodus-Routen einrichten
  setupMinimalRoutes();

  _server->begin();
  LOG_INFO(F("WebManager"), F("Update-Server im Minimalmodus gestartet"));
  logger.logMemoryStats(F("update_modus_abgeschlossen"));

  _initialized = true;
  return ResourceResult::success();
}

ResourceResult WebManager::setupMinimalServices() {
  // Dienste in bestimmter Reihenfolge anlegen
  _server = std::make_unique<ESPWebServer>(_port);
  // ESP8266WebServer hebt nur ausdrücklich angeforderte Kopfzeilen auf. Ohne
  // das hier liefert server.header("Accept-Encoding") immer eine leere
  // Zeichenkette, und tryServeStaticFile() könnte nicht erkennen, ob der
  // Browser gzip verträgt.
  collectStaticHeaders();
  if (!_server) {
    return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                F("Webserver konnte nicht angelegt werden"));
  }

  _auth = std::make_unique<WebAuth>(*_server);
  if (!_auth) {
    return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                F("Authentifizierungsdienst konnte nicht angelegt werden"));
  }

  _router = std::make_unique<WebRouter>(*_server);
  if (!_router) {
    return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                F("Webrouter konnte nicht angelegt werden"));
  }

  // OTA-Handler ohne Template-Engine-Abhängigkeit erstellen
  _otaHandler = std::make_unique<WebOTAHandler>(*_server, *_auth);
  if (!_otaHandler) {
    return ResourceResult::fail(ResourceError::RESOURCE_ERROR,
                                F("OTA-Handler konnte nicht angelegt werden"));
  }

  // Statische Dateien laufen auch im Minimalmodus über tryServeStaticFile()
  // im onNotFound-Handler (siehe setupMinimalRoutes()).

  _server->begin();

  return ResourceResult::success();
}

ResourceResult WebManager::setupServices() {
  // Statische Dateien werden nicht mehr einzeln registriert.
  //
  // Hier standen 21 Aufrufe von _server->on() - je einer pro CSS-, JS- und
  // Bilddatei. Jeder davon legt im ESP8266WebServer einen RequestHandler an:
  // ein Listenknoten mit eigenem String-URI und einer std::function, zusammen
  // rund 1,3 KB Heap, nur um Dateien auszuliefern, deren Pfad direkt aus der
  // URL folgt.
  //
  // Die Auslieferung übernimmt jetzt tryServeStaticFile() im
  // onNotFound-Handler (siehe setupRoutes() bzw. setupMinimalRoutes()).
  LOG_INFO(F("WebManager"), F("Statische Dateien werden über den Asset-Handler ausgeliefert"));
  return ResourceResult::success();
}

void WebManager::setupMiddleware() {
  LOG_DEBUG(F("WebManager"), F("Middleware wird eingerichtet..."));

  // Middleware: Whitelist statt Blacklist. Nur explizit als öffentlich gelistete
  // Pfade sind ohne Anmeldung erreichbar, alles andere verlangt Authentifizierung.
  // Vorher wurde nur /admin* geprüft — dadurch war z.B. /logs frei zugänglich.
  _router->addMiddleware([this](HTTPMethod method, String url) {
    // Öffentliche Routen: Startseite, deren Live-Daten und statische Assets.
    // /status ist bewusst dabei: die Oberfläche pollt es nach jedem Neustart, um
    // festzustellen, wann das Gerät wieder da ist - unter anderem nach dem
    // Zurücksetzen, das Admin-Passwort und WLAN-Zugangsdaten mit zurücksetzt und
    // eine angemeldete Abfrage damit unmöglich macht. Im Minimalmodus ist /status
    // ohnehin schon frei, weil beginUpdateMode() keine Middleware einrichtet.
    // /measure ist ebenfalls bewusst offen: ein Klick auf ein Sensorblatt der
    // Startseite zieht die nächste Messung dieses Sensors vor. Das ändert keine
    // Konfiguration und legt nichts offen, was /getLatestValues nicht ohnehin
    // zeigt; gegen Dauerbeschuss schützt die Drossel in
    // SensorHandler::handleMeasure().
    // /chronik zeigt den Verlauf derselben Messwerte, die die Startseite ohnehin
    // öffentlich anzeigt - ein Passwortdialog davor verhinderte vor allem die
    // spontane Nutzung am Gerät. Der teuerste der drei Pfade, /chronik/export.csv,
    // ist in ChronikHandler::handleExport() gedrosselt.
    if (url == "/" || url == "/status" || url == "/getLatestValues" || url == "/measure" ||
        url.startsWith("/chronik") || url.startsWith("/css/") || url.startsWith("/js/") ||
        url.startsWith("/img/") || url.startsWith("/favicon")) {
      return true;
    }

    // Alles Übrige benötigt Authentifizierung
    if (!WebAuth::checkAdminCredentials(*_server)) {
      _server->requestAuthentication();
      return false;
    }
    return true;
  });

  // Logging-Middleware hinzufügen
  _router->addMiddleware([this](HTTPMethod method, String url) {
    LOG_DEBUG(F("WebManager"),
              String(F("Anfrage: ")) + methodToString(method) + String(F(" ")) + url);
    return true;
  });

  LOG_DEBUG(F("WebManager"), F("Middleware-Konfiguration abgeschlossen"));
}
