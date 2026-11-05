/**
 * @file improv.cpp
 */
#include "core/net/improv.h"

#include <string.h>

namespace ps {
namespace improv {

namespace {
const char kHeader[6] = {'I', 'M', 'P', 'R', 'O', 'V'};

uint8_t checksum(const uint8_t* p, size_t n) {
  uint8_t s = 0;
  for (size_t i = 0; i < n; i++)
    s = static_cast<uint8_t>(s + p[i]);
  return s;
}

size_t packet(uint8_t* out, size_t cap, Type t, const uint8_t* data, size_t n) {
  if (n > 255 || cap < 6 + 3 + n + 1)
    return 0;
  memcpy(out, kHeader, 6);
  out[6] = kVersion;
  out[7] = static_cast<uint8_t>(t);
  out[8] = static_cast<uint8_t>(n);
  if (n)
    memcpy(out + 9, data, n);
  out[9 + n] = checksum(out, 9 + n);
  return 10 + n;
}

/// Zeichenkette mit Längenbyte lesen
bool readString(const uint8_t* p, size_t avail, size_t& pos, char* out, size_t cap) {
  if (pos >= avail)
    return false;
  const size_t n = p[pos++];
  if (n >= cap || pos + n > avail)
    return false;
  memcpy(out, p + pos, n);
  out[n] = '\0';
  pos += n;
  return true;
}
} // namespace

Parser::Result Parser::feed(uint8_t b, Command& out) {
  if (len_ < 6) {
    if (b == static_cast<uint8_t>(kHeader[len_])) {
      buf_[len_++] = b;
    } else {
      len_ = b == static_cast<uint8_t>(kHeader[0]) ? 1 : 0;
      if (len_)
        buf_[0] = b;
    }
    return Result::None;
  }
  buf_[len_++] = b;
  if (len_ == 7 && b != kVersion) {
    len_ = 0;
    return Result::Invalid;
  }
  if (len_ == 9 && buf_[8] > kMaxData) {
    len_ = 0;
    return Result::Invalid;
  }
  if (len_ >= 9 && len_ == 10u + buf_[8])
    return finish(out);
  return Result::None;
}

Parser::Result Parser::finish(Command& out) {
  const size_t n = buf_[8];
  const bool ok = checksum(buf_, 9 + n) == buf_[9 + n];
  const uint8_t type = buf_[7];
  const uint8_t* d = buf_ + 9;
  len_ = 0;
  if (!ok || type != static_cast<uint8_t>(Type::Rpc) || n < 2 || d[1] + 2u != n)
    return Result::Invalid;
  out = Command{};
  const uint8_t cmd = d[0];
  switch (cmd) {
  case static_cast<uint8_t>(Cmd::WifiSettings): {
    size_t pos = 2;
    if (!readString(d, n, pos, out.ssid, sizeof(out.ssid)) || out.ssid[0] == '\0' ||
        !readString(d, n, pos, out.password, sizeof(out.password)))
      return Result::Invalid;
    out.cmd = Cmd::WifiSettings;
    return Result::Command;
  }
  case static_cast<uint8_t>(Cmd::GetState):
  case static_cast<uint8_t>(Cmd::GetInfo):
  case static_cast<uint8_t>(Cmd::GetNetworks):
    out.cmd = static_cast<Cmd>(cmd);
    return Result::Command;
  default:
    return Result::Unknown;
  }
}

size_t encodeState(uint8_t* out, size_t cap, State s) {
  const uint8_t d = static_cast<uint8_t>(s);
  return packet(out, cap, Type::CurrentState, &d, 1);
}

size_t encodeError(uint8_t* out, size_t cap, Error e) {
  const uint8_t d = static_cast<uint8_t>(e);
  return packet(out, cap, Type::ErrorState, &d, 1);
}

size_t encodeResult(uint8_t* out, size_t cap, Cmd cmd, const char* const* strings, uint8_t n) {
  uint8_t d[kMaxData + 64];
  size_t len = 2;
  for (uint8_t i = 0; i < n; i++) {
    const size_t sl = strlen(strings[i]);
    if (sl > 255 || len + 1 + sl > sizeof(d))
      return 0;
    d[len++] = static_cast<uint8_t>(sl);
    memcpy(d + len, strings[i], sl);
    len += sl;
  }
  d[0] = static_cast<uint8_t>(cmd);
  d[1] = static_cast<uint8_t>(len - 2);
  return packet(out, cap, Type::RpcResult, d, len);
}

} // namespace improv
} // namespace ps
