/**
 * @file main.cpp
 * @brief Composition Root: alle Dienste als statische Objekte, setup(), loop()
 * @details Einzige Stelle mit globalen Objekten (refactor.md 5.1 Punkt 5).
 *          Abhängigkeiten werden per Konstruktor weitergereicht. Flash wird
 *          nur in persist() geschrieben; Neustarts passieren erst, nachdem
 *          alles gesichert ist (ersetzt die delay/handleClient-Hacks, B-54).
 */
#include <Arduino.h>

#include <type_traits>

#include "app/boot_guard.h"
#include "app/scheduler.h"
#include "core/version.h"
#include "drivers/analog_mux.h"
#include "drivers/bmp280.h"
#include "drivers/dht.h"
#include "drivers/ds18b20.h"
#include "drivers/hx711.h"
#include "drivers/led_traffic.h"
#include "drivers/mhz19.h"
#include "drivers/sds011.h"
#include "drivers/serial_receiver.h"
#include "drivers/ssd1306.h"
#include "platform/arduino/fs_littlefs.h"
#include "platform/arduino/ota_port.h"
#include "platform/arduino/tcp_client.h"
#include "platform/arduino/web_server.h"
#include "platform/arduino/wifi_port.h"
#include "services/chronik_service.h"
#include "services/config_service.h"
#include "services/display_service.h"
#include "services/improv_service.h"
#include "services/led_service.h"
#include "services/log_service.h"
#include "services/mail_service.h"
#include "services/metrics_service.h"
#include "services/net_service.h"
#include "services/ota_service.h"
#include "services/provision_service.h"
#include "services/sensor_service.h"
#include "services/state_store.h"
#include "services/storage.h"
#include "services/system_info.h"
#include "services/template_store.h"
#include "services/time_service.h"
#include "web/auth.h"
#include "web/router.h"
#include "web/uploads.h"

using namespace ps;

namespace {

// ---------------------------------------------------------------------------
// Treiber: alle sind einkompiliert, angemeldet werden die bestückten
// (board::hardware() - Webflasher oder Profil-Voreinstellung)
// ---------------------------------------------------------------------------
drivers::AnalogMux g_analog;
drivers::Dht g_dht;
drivers::Ds18b20 g_ds18b20;
drivers::Sds011 g_sds011;
drivers::Mhz19 g_mhz19;
drivers::Hx711 g_hx711;
drivers::Bmp280 g_bmp280;
drivers::SerialReceiver g_serial;

SensorDriver* driverFor(SensorType t) {
  switch (t) {
  case SensorType::Analog:
    return &g_analog;
  case SensorType::Dht:
    return &g_dht;
  case SensorType::Ds18b20:
    return &g_ds18b20;
  case SensorType::Sds011:
    return &g_sds011;
  case SensorType::Mhz19:
    return &g_mhz19;
  case SensorType::Hx711:
    return &g_hx711;
  case SensorType::Bmp280:
    return &g_bmp280;
  case SensorType::SerialReceiver:
    return &g_serial;
  default:
    return nullptr;
  }
}

// ---------------------------------------------------------------------------
// Plattform und Dienste
// ---------------------------------------------------------------------------
platform::LittleFs g_fs;
platform::WifiPort g_wifi;
platform::TcpClient g_tcp;
platform::OtaPort g_otaPort;
platform::WebServer g_http;
drivers::Ssd1306 g_displayBus;
drivers::LedTraffic g_ledPins;

Events g_events;
BootGuard g_boot;
LogService g_log(g_fs);
TemplateStore g_templates(g_fs);
StateStore g_state(g_fs);
ConfigService g_cfg(g_fs, g_templates, g_state);
SystemInfo g_sys(g_fs);
SensorService g_sensors(g_cfg, g_state, g_events);
ChronikService g_chronik(g_fs, g_cfg, g_sensors);
NetService g_net(g_wifi, g_cfg, g_events);
TimeService g_time(g_events);
DisplayService g_display(g_displayBus, g_cfg, g_sensors, g_net);
LedService g_led(g_ledPins, g_cfg, g_sensors);
MailService g_mail(g_tcp, g_cfg, g_state, g_templates, g_sensors, g_net, g_sys);
MetricsService g_metrics(g_cfg, g_sensors, g_net, g_sys);
OtaService g_ota(g_otaPort, g_cfg, g_events);
Auth g_auth(g_cfg);
ProvisionService g_provision;
ImprovService g_improv(g_cfg, g_net);
WebContext g_webCtx{g_cfg, g_sensors, g_state,   g_chronik, g_log,  g_mail,   g_templates,
                    g_net, g_sys,     g_metrics, g_ota,     g_auth, g_events, g_fs};
web::Router g_router(g_webCtx);
web::OtaUpload g_otaUpload(g_webCtx, g_router);
web::ImportUpload g_importUpload(g_webCtx, g_router);

// Ausstehender Neustart / Werksreset (nach dem Senden der Antwort)
enum class Pending : uint8_t { None, Reboot, Reset };
Pending g_pending = Pending::None;
Instant g_pendingAt;
bool g_bootScreen = true;
Instant g_bootDoneAt;
bool g_bootDoneSeen = false;

Instant clockNow() { return hal::now(); }

// ---------------------------------------------------------------------------
// Persistenz: einziger Flash-Schreibpfad
// ---------------------------------------------------------------------------
void persist(Instant now, bool force) {
  g_cfg.tick(now, force);
  g_state.tick(now, force);
  g_chronik.tick(now, force);
  g_log.flush(force);
  g_sys.persist();
}

void taskPersist(void*, Instant now) {
  if (g_pending != Pending::None && now.reached(g_pendingAt)) {
    persist(now, true);
    if (g_pending == Pending::Reset)
      g_cfg.factoryReset();
    LOGW(Mod::Main, "Neustart");
    g_log.flush(true);
    hal::restart();
    return;
  }
  persist(now, false);
}

void requestRestart(Pending p, Instant now) {
  if (g_pending == Pending::Reset)
    return; // Werksreset hat Vorrang
  g_pending = p;
  g_pendingAt = now + Duration::ms(800); // Antwort zuerst rausschicken
}

// ---------------------------------------------------------------------------
// Ereignisse
// ---------------------------------------------------------------------------
void handleEvents(void*, Instant now) {
  Event e;
  while (g_events.pop(e)) {
    switch (e.type) {
    case EventType::MeasurementDone:
      g_chronik.record(e.a, hal::epoch(), now);
      break;
    case EventType::ConfigChanged:
      if (e.a & SecLog) {
        const bool wasFile = g_log.fileLogActive();
        g_log.configure(g_cfg.device());
        if (wasFile != g_log.fileLogActive())
          g_chronik.requestBudgetRecheck();
      }
      if (e.a & (SecWifi | SecGeneral))
        g_net.configChanged(now, e.v == 1);
      if (e.a & SecMail)
        g_mail.configChanged();
      if (e.a & (SecDisplay | SecGeneral))
        g_display.refresh();
      if (e.a & SecLed)
        g_led.tick(now);
      break;
    case EventType::SensorConfigChanged:
      g_display.refresh();
      break;
    case EventType::WifiUp:
    case EventType::ApStarted:
      g_display.refresh();
      break;
    case EventType::RebootRequested:
      requestRestart(Pending::Reboot, now);
      break;
    case EventType::FactoryResetRequested:
      requestRestart(Pending::Reset, now);
      break;
    default:
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------
void taskWeb(void*, Instant) { g_http.handle(); }
void taskNet(void*, Instant now) {
  g_net.tick(now);
  if (g_bootScreen && g_net.state() != NetService::State::TrySlot) {
    if (!g_bootDoneSeen) {
      g_bootDoneSeen = true;
      g_bootDoneAt = now + Duration::s(3);
      g_display.bootLine("Setup abgeschlossen");
    } else if (now.reached(g_bootDoneAt)) {
      g_bootScreen = false;
      g_display.endBoot();
    }
  }
}
void taskSensors(void*, Instant now) { g_sensors.tick(now); }
void taskLed(void*, Instant now) {
  if (board::features().ledTraffic)
    g_led.tick(now);
}
void taskDisplay(void*, Instant now) { g_display.tick(now); }
void taskTime(void*, Instant now) {
  g_time.tick();
  g_boot.tick(now);
  g_ota.tick(now);
}
void taskMail(void*, Instant now) { g_mail.tick(now); }
// USB: WLAN-Einrichtung durch den Webflasher (Improv Serial)
void taskImprov(void*, Instant now) { g_improv.tick(now); }
void reportLoop();
void taskSystem(void*, Instant now) {
  g_sys.tick(now);
  reportLoop();
}

Task g_tasks[] = {
    {handleEvents, nullptr, 0, Instant(), "events"},
    {taskWeb, nullptr, 0, Instant(), "web"},
    {taskNet, nullptr, 100, Instant(), "net"},
    // 10 ms: der DHT11 verlangt 18-30 ms Startsignal; der Leerlauf-Tick kostet µs
    {taskSensors, nullptr, 10, Instant(), "sensors"},
    {taskDisplay, nullptr, 100, Instant(), "display"},
    {taskMail, nullptr, 100, Instant(), "mail"},
    {taskImprov, nullptr, 50, Instant(), "improv"},
    {taskLed, nullptr, 1000, Instant(), "led"},
    {taskTime, nullptr, 1000, Instant(), "time"},
    {taskPersist, nullptr, 1000, Instant(), "persist"},
    {taskSystem, nullptr, 30000, Instant(), "system"},
};
TaskScheduler g_scheduler(g_tasks, sizeof(g_tasks) / sizeof(g_tasks[0]));

// Debug "RAM": längste Schleife der letzten 30 s (Abnahme AP-1: < 20 ms)
void reportLoop() {
  const char* task = "";
  const int32_t m = g_scheduler.takeWindowMax(&task);
  LOGD(Mod::Mem, "Loop max. %ld ms in 30 s (%s)", static_cast<long>(m), task);
}

// OTA-Haken: Sensoren pausieren und alles sichern
void otaPrepare(void*, bool starting) {
  const Instant now = hal::now();
  if (starting) {
    g_sensors.pause(true);
    persist(now, true);
    g_display.bootHeader(true);
    g_display.otaProgress(0);
  } else {
    g_sensors.pause(false);
    g_display.endBoot();
  }
}
void otaProgress(void*, uint8_t p) { g_display.otaProgress(p); }
int displaySlot(void*) { return g_display.currentSlot(); }
void bootLogLine(void*, const char* line) { g_display.bootLine(line); }

} // namespace

void setup() {
  hal::consoleBegin(115200);
  log::attach(&g_log);
  // 1. Hardware: aus dem Webflasher-Block, sonst Voreinstellung des Profils.
  //    Vor allem anderen - Kanal-Layout und Konfiguration hängen davon ab.
  g_provision.loadHardware();
  LOGI(Mod::Boot, "Pflanzensensor %s (%s, %s) startet", kVersion, board::name(), kBuildDate);

  // 2. Dateisystem
  if (!g_fs.mount()) {
    LOGE(Mod::Fs, "LittleFS nicht einhängbar - wird formatiert");
    g_fs.format();
    if (!g_fs.mount())
      LOGE(Mod::Fs, "LittleFS auch nach dem Formatieren nicht nutzbar");
  }
  {
    const storage::CleanupResult cr = storage::removeForeign(g_fs);
    if (cr.removed)
      LOGW(Mod::Fs, "%u Altlasten der alten Firmware gelöscht, %lu KB frei geworden", cr.removed,
           static_cast<unsigned long>(cr.freedBytes / 1024));
  }
  // 3. Boot-Schleifen-Schutz
  g_boot.begin(hal::resetReason());
  // 4. Konfiguration
  g_cfg.load();
  g_provision.applySettings(g_cfg);
  g_webCtx.provision = &g_provision;
  g_log.configure(g_cfg.device());
  if (g_boot.fileLogBlocked()) {
    LOGW(Mod::Boot, "Mehrfachstart: Datei-Log aus und gelöscht");
    g_log.blockFileLog();
    g_log.deleteFile();
  }
  // 5. Neustartzähler
  g_sys.begin();
  // 6. Display und Bootbildschirm
  g_display.begin();
  // 7. WLAN (nicht blockierend)
  g_net.setBootLog(bootLogLine, nullptr);
  g_net.begin(hal::now());
  // 8. NTP
  g_time.begin();
  // 9. LED-Ampel
  if (board::features().ledTraffic) {
    g_ledPins.begin();
    g_led.setDisplaySource(displaySlot, nullptr);
  }
  // 10. Sensoren (ein fehlschlagendes begin() lässt den Sensor im Plan, B-10)
  if (!g_state.load())
    LOGI(Mod::Config, "Kein gespeicherter Kanalzustand");
  SensorDriver* drivers[board::kMaxSensors] = {};
  for (uint8_t i = 0; i < board::sensorCount(); i++)
    drivers[i] = driverFor(board::sensor(i).type);
  g_sensors.setRestartGate(BootGuard::sensorRestartGate, &g_boot);
  g_sensors.begin(drivers, hal::now());
  // 11. Chronik
  g_chronik.begin(g_boot.chronikBlocked());
  // 12. Webserver (mDNS kommt mit der WLAN-Verbindung)
  g_ota.setHooks(otaPrepare, otaProgress, nullptr);
  g_http.addUpload("/api/v1/ota", &g_otaUpload);
  g_http.addUpload("/api/v1/config/import", &g_importUpload);
  g_http.begin(80, web::Router::dispatch, &g_router);
  // 13. Mail
  g_mail.begin(hal::now());
  g_scheduler.start(hal::now());
  LOGI(Mod::Boot, "Start abgeschlossen, %lu B Heap frei",
       static_cast<unsigned long>(hal::heap().free));
}

void loop() {
  g_scheduler.run(clockNow);
  hal::yieldNow();
}
