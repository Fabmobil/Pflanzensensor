/**
 * @file mail_service.h
 * @brief Mailbenachrichtigungen über den Mailbot (refactor.md 2.11, 3.3, 5.15)
 * @details Ein Puffer (≤ 2,8 KB, nur während des Versands): Klartext-JSON mit
 *          direkt hineingerendertem Rumpf, in place verschlüsselt; Umschlag
 *          und Base64 gehen gestreamt in den Socket. Die Antwort wird
 *          nicht-blockierend in poll() gelesen.
 */
#pragma once

#include "core/mail/scheduler.h"
#include "hal/tcp_client.h"
#include "services/config_service.h"
#include "services/net_service.h"
#include "services/sensor_service.h"
#include "services/system_info.h"
#include "services/template_store.h"

namespace ps {

class MailService {
public:
  static constexpr uint32_t kCheckMs = 30000;
  static constexpr uint32_t kTestCheckMs = 5000;
  static constexpr uint32_t kTestPatienceMs = 120000;
  static constexpr uint32_t kResponseTimeoutMs = 10000;
  static constexpr uint32_t kConnectTimeoutMs = 5000;
  static constexpr size_t kBufferSize = 2800;

  MailService(hal::TcpClient& tcp, ConfigService& cfg, StateStore& state, TemplateStore& tpl,
              SensorService& sensors, NetService& net, SystemInfo& sys)
      : tcp_(tcp), cfg_(cfg), state_(state), tpl_(tpl), sensors_(sensors), net_(net), sys_(sys) {}

  void begin(Instant now);
  void configChanged();
  /// Aus der Hauptschleife (alle 100 ms); prüft selbst seine Takte
  void tick(Instant now);
  void requestTest(Instant now);
  void writeStatus(JsonWriter& w) const;

  /// Rumpf rendern (auch für Tests): schreibt HTML in out
  void renderBody(Mail::Kind kind, Sink& out);
  size_t renderSubject(Mail::Kind kind, char* out, size_t cap);

private:
  enum class Phase : uint8_t { Idle, Waiting };
  void updateLevels();
  Mail::SchedulerConfig schedulerConfig() const;
  /// Verbindung aufbauen und Anfrage senden; false mit Fehlertext
  bool startSend(Mail::Kind kind, Instant now);
  void finishSend(bool ok, int httpStatus, const char* message, Instant now);
  void pollResponse(Instant now);

  hal::TcpClient& tcp_;
  ConfigService& cfg_;
  StateStore& state_;
  TemplateStore& tpl_;
  SensorService& sensors_;
  NetService& net_;
  SystemInfo& sys_;
  Mail::Scheduler sched_;

  Phase phase_ = Phase::Idle;
  Mail::Kind sending_ = Mail::Kind::None;
  bool sendingTest_ = false;
  bool testPending_ = false;
  Instant testDeadline_;
  Instant lastCheck_;
  bool checkedOnce_ = false;
  Instant waitSince_;
  char resp_[200] = "";
  size_t respLen_ = 0;

  uint32_t sent_ = 0;
  uint32_t lastAttempt_ = 0;
  FixedString<120> lastResult_;
};

} // namespace ps
