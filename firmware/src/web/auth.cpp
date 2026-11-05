/**
 * @file auth.cpp
 */
#include "web/auth.h"

#include "core/util/base64.h"
#include "core/util/sha256.h"
#include "hal/system.h"
#include "services/config_service.h"
#include "services/log.h"

namespace ps {

namespace {
/**
 * SHA-256 des Notfall-Adminpassworts aus dem Altcode (web_auth.h,
 * EMERGENCY_ADMIN_PASSWORD). Der Wert ist im öffentlichen Repository bekannt
 * und bleibt per Nutzerentscheidung (E9, Befund B-32). Gespeichert ist nur der
 * Hash, damit er nicht per "strings" aus der firmware.bin fällt.
 */
const uint8_t kEmergencyHash[32] = {
    0xfc, 0xbb, 0x3c, 0x3e, 0xbd, 0x99, 0xce, 0x52, 0x60, 0x59, 0xdb, 0xd4, 0x8e, 0x2b, 0x09, 0x46,
    0x8f, 0x38, 0x72, 0x0a, 0xbc, 0xec, 0x16, 0x67, 0x42, 0xf4, 0xd7, 0xe7, 0xb8, 0xbd, 0x9d, 0xb4};
} // namespace

bool Auth::isEmergencyPassword(const char* pw) {
  if (!pw)
    return false;
  Sha256 h;
  h.update(pw);
  uint8_t d[32];
  h.finish(d);
  return equalConstTime(d, kEmergencyHash, 32);
}

bool Auth::passwordOk(const char* password, bool* emergency) const {
  if (emergency)
    *emergency = false;
  if (!password)
    return false;
  if (cfg_.checkAdminPassword(password))
    return true;
  if (isEmergencyPassword(password)) {
    if (emergency)
      *emergency = true;
    return true;
  }
  return false;
}

uint32_t Auth::lockedFor(uint32_t nowMs) const {
  if (!locked_)
    return 0;
  const int32_t left = static_cast<int32_t>(lockedUntil_ - nowMs);
  return left > 0 ? static_cast<uint32_t>(left) : 0;
}

Auth::Result Auth::verify(const char* password, uint32_t nowMs) {
  if (locked_ && lockedFor(nowMs) == 0) {
    locked_ = false;
    failures_ = 0;
  }
  if (locked_)
    return Result::Locked;
  bool emergency = false;
  if (passwordOk(password, &emergency)) {
    failures_ = 0;
    if (emergency && emergencyWarn_.allow(nowMs))
      LOGW(Mod::Auth, "Zugriff über das Notfallpasswort - bitte ein neues Adminpasswort vergeben");
    return Result::Ok;
  }
  if (++failures_ >= kMaxFailures) {
    locked_ = true;
    lockedUntil_ = nowMs + kLockMs;
    LOGW(Mod::Auth, "%u Fehlversuche - Anmeldung 30 s gesperrt", failures_);
  }
  return Result::Wrong;
}

namespace {
int hexVal(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

/// Token aus "…; ps_session=<32 hex>; …"
bool cookieToken(const hal::Request& req, uint8_t out[16]) {
  char cookie[200];
  if (!req.header("Cookie", cookie, sizeof(cookie)))
    return false;
  const size_t nameLen = strlen(Auth::kCookie);
  for (const char* p = cookie; (p = strstr(p, Auth::kCookie)) != nullptr; p += nameLen) {
    if (p != cookie && p[-1] != ' ' && p[-1] != ';')
      continue;
    if (p[nameLen] != '=')
      continue;
    const char* v = p + nameLen + 1;
    for (uint8_t i = 0; i < 16; i++) {
      const int hi = hexVal(v[2 * i]), lo = hexVal(v[2 * i + 1]);
      if (hi < 0 || lo < 0)
        return false;
      out[i] = static_cast<uint8_t>(hi << 4 | lo);
    }
    return true;
  }
  return false;
}
} // namespace

int Auth::sessionOf(const hal::Request& req, uint32_t nowMs) const {
  uint8_t t[16];
  if (!cookieToken(req, t))
    return -1;
  for (uint8_t i = 0; i < kSessions; i++) {
    const Session& s = sessions_[i];
    if (!s.used || nowMs - s.lastUse > kSessionIdleMs)
      continue;
    if (equalConstTime(t, s.token, 16))
      return i;
  }
  return -1;
}

Auth::Result Auth::login(const char* password, uint32_t nowMs, char token[kTokenHex + 1]) {
  const Result r = verify(password, nowMs);
  if (r != Result::Ok)
    return r;
  // Freie oder am längsten unbenutzte Sitzung
  uint8_t pick = 0;
  for (uint8_t i = 0; i < kSessions; i++) {
    if (!sessions_[i].used) {
      pick = i;
      break;
    }
    if (nowMs - sessions_[i].lastUse > nowMs - sessions_[pick].lastUse)
      pick = i;
  }
  Session& s = sessions_[pick];
  hal::fillRandom(s.token, sizeof(s.token));
  s.used = true;
  s.lastUse = nowMs;
  static const char kHex[] = "0123456789abcdef";
  for (uint8_t i = 0; i < 16; i++) {
    token[2 * i] = kHex[s.token[i] >> 4];
    token[2 * i + 1] = kHex[s.token[i] & 15];
  }
  token[kTokenHex] = '\0';
  LOGI(Mod::Auth, "Angemeldet");
  return Result::Ok;
}

void Auth::logout(const hal::Request& req) {
  uint8_t t[16];
  if (!cookieToken(req, t))
    return;
  for (auto& s : sessions_)
    if (s.used && equalConstTime(t, s.token, 16))
      s = Session{};
}

void Auth::revokeOthers(const hal::Request& req) {
  uint8_t t[16];
  const bool have = cookieToken(req, t);
  for (auto& s : sessions_)
    if (s.used && !(have && equalConstTime(t, s.token, 16)))
      s = Session{};
}

Auth::Result Auth::check(const hal::Request& req, uint32_t nowMs) {
  const int si = sessionOf(req, nowMs);
  if (si >= 0) {
    sessions_[si].lastUse = nowMs;
    return Result::Ok;
  }
  char header[160];
  if (!req.header("Authorization", header, sizeof(header)) || strncmp(header, "Basic ", 6) != 0)
    return locked_ && lockedFor(nowMs) > 0 ? Result::Locked : Result::Missing;
  char decoded[120];
  const int n = base64::decode(header + 6, strlen(header + 6), reinterpret_cast<uint8_t*>(decoded),
                               sizeof(decoded) - 1);
  if (n <= 0)
    return Result::Wrong;
  decoded[n] = '\0';
  char* colon = strchr(decoded, ':');
  if (!colon)
    return Result::Wrong;
  // Benutzername egal - es gibt nur ein Passwort
  const Result r = verify(colon + 1, nowMs);
  memset(decoded, 0, sizeof(decoded));
  return r;
}

} // namespace ps
