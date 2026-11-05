/**
 * @file improv.h
 * @brief Improv-Serial-Protokoll v1 (https://www.improv-wifi.com/serial/)
 * @details Der Webflasher (ESP Web Tools) richtet darüber nach dem Flashen
 *          das WLAN ein. Paket: "IMPROV" | Version 1 | Typ | Länge | Daten |
 *          Prüfsumme (Summe aller Bytes davor, 8 Bit). Arduino-frei, nativ
 *          getestet.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {
namespace improv {

enum class State : uint8_t { Ready = 0x02, Provisioning = 0x03, Provisioned = 0x04 };
enum class Error : uint8_t {
  None = 0x00,
  InvalidRpc = 0x01,
  UnknownRpc = 0x02,
  UnableToConnect = 0x03,
  Unknown = 0xFF
};
enum class Type : uint8_t { CurrentState = 0x01, ErrorState = 0x02, Rpc = 0x03, RpcResult = 0x04 };
enum class Cmd : uint8_t {
  WifiSettings = 0x01,
  GetState = 0x02,
  GetInfo = 0x03,
  GetNetworks = 0x04
};

constexpr uint8_t kVersion = 1;
constexpr size_t kMaxData = 128;
constexpr size_t kMaxPacket = 6 + 3 + kMaxData + 1;

struct Command {
  Cmd cmd = Cmd::GetState;
  char ssid[33] = {};
  char password[65] = {};
};

/// Liest Byte für Byte; alles außerhalb von Paketen (Rauschen) wird verworfen.
class Parser {
public:
  enum class Result : uint8_t { None, Command, Invalid, Unknown };
  /// Invalid: kaputtes Paket (Prüfsumme, Länge); Unknown: RPC mit unbekanntem Befehl
  Result feed(uint8_t b, Command& out);

private:
  Result finish(Command& out);
  uint8_t buf_[kMaxPacket] = {};
  size_t len_ = 0;
};

/// Pakete bauen; Rückgabe: Länge, 0 wenn cap zu klein
size_t encodeState(uint8_t* out, size_t cap, State s);
size_t encodeError(uint8_t* out, size_t cap, Error e);
/// RPC-Ergebnis mit n Zeichenketten
size_t encodeResult(uint8_t* out, size_t cap, Cmd cmd, const char* const* strings, uint8_t n);

} // namespace improv
} // namespace ps
