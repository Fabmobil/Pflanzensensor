/**
 * @file auth.h
 * @brief Anmeldung nur mit Passwort (refactor.md 5.9, E9)
 * @details Es gibt keine Benutzer, nur ein Adminpasswort. Der Browser meldet
 *          sich über /login an und bekommt ein Sitzungs-Cookie (HttpOnly,
 *          SameSite=Strict, im RAM - nach einem Neustart neu anmelden).
 *          Skripte dürfen weiter HTTP-Basic-Auth schicken; der Benutzername
 *          wird dabei ignoriert.
 *          Geprüft wird das eingestellte Passwort (gesalzener SHA-256) und
 *          das fest einkompilierte Notfallpasswort des Altcodes. Letzteres
 *          steht nur als Hash im Image. Nach 5 Fehlversuchen 30 s Sperre.
 */
#pragma once

#include "core/types.h"
#include "core/util/throttle.h"
#include "hal/http_server.h"

namespace ps {

class ConfigService;

class Auth {
public:
  static constexpr uint8_t kMaxFailures = 5;
  static constexpr uint32_t kLockMs = 30000;
  static constexpr uint32_t kEmergencyWarnMs = 60000;

  static constexpr uint8_t kSessions = 4;
  static constexpr uint32_t kSessionIdleMs = 14UL * 24 * 3600 * 1000; ///< < 24,8 Tage (int32)
  static constexpr size_t kTokenHex = 32;
  static constexpr const char* kCookie = "ps_session";

  explicit Auth(ConfigService& cfg) : cfg_(cfg) {}

  enum class Result : uint8_t { Ok, Missing, Wrong, Locked };
  /// Sitzungs-Cookie oder Basic-Auth (Benutzername egal)
  Result check(const hal::Request& req, uint32_t nowMs);
  /// Passwort prüfen (mit Sperrlogik) und eine Sitzung anlegen.
  /// token bekommt 32 Hex-Zeichen + NUL.
  Result login(const char* password, uint32_t nowMs, char token[kTokenHex + 1]);
  /// Sitzung des Cookies beenden
  void logout(const hal::Request& req);
  /// Alle Sitzungen außer der des Anfragenden beenden (nach Passwortwechsel)
  void revokeOthers(const hal::Request& req);
  /// Prüft das Passwort (ohne Sperrlogik, für Tests)
  bool passwordOk(const char* password, bool* emergency = nullptr) const;
  static bool isEmergencyPassword(const char* pw);
  /// Restliche Sperrzeit in ms
  uint32_t lockedFor(uint32_t nowMs) const;

private:
  struct Session {
    bool used = false;
    uint8_t token[16] = {};
    uint32_t lastUse = 0;
  };
  /// Passwort mit Fehlversuchszählung prüfen
  Result verify(const char* password, uint32_t nowMs);
  /// Index der Sitzung aus dem Cookie oder -1
  int sessionOf(const hal::Request& req, uint32_t nowMs) const;
  Session sessions_[kSessions];
  ConfigService& cfg_;
  uint8_t failures_ = 0;
  uint32_t lockedUntil_ = 0;
  bool locked_ = false;
  RateGate emergencyWarn_{kEmergencyWarnMs};
};

} // namespace ps
