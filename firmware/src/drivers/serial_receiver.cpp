/**
 * @file serial_receiver.cpp
 */
#if defined(ARDUINO)

#include "drivers/serial_receiver.h"

#include <stdio.h>

#include "board.h"
#include "platform/arduino/uart_port.h"

namespace ps {
namespace drivers {

namespace {
platform::UartPort& port() {
  static platform::UartPort p(2);
  return p;
}
} // namespace

SensorInfo SerialReceiver::info() const {
  SensorInfo i = catalog::info(SensorType::SerialReceiver);
  i.roundTimeoutMs = static_cast<uint16_t>(board::features().serialTimeoutMs * 7 + 1000);
  return i;
}

Step SerialReceiver::begin(Instant) {
  if (!begun_)
    begun_ =
        port().begin(board::features().serialBaud, board::pins().serialRx, board::pins().serialTx);
  return begun_ ? Step::Ok : Step::Error;
}

void SerialReceiver::request(uint8_t idx, Instant now) {
  char cmd[12];
  const int len = snprintf(cmd, sizeof(cmd), "GET:%u\n", idx);
  port().write(reinterpret_cast<const uint8_t*>(cmd), static_cast<size_t>(len));
  sentAt_ = now;
  line_.reset();
}

Step SerialReceiver::startRound(Instant now) {
  port().flushInput();
  idx_ = 0;
  request(0, now);
  return Step::Ok;
}

Step SerialReceiver::pollRound(Instant now, Reading* out, uint8_t n) {
  while (idx_ < n) {
    bool done = false;
    while (port().available() > 0) {
      if (line_.feed(static_cast<char>(port().read()))) {
        float v;
        const ChannelDef d = catalog::channel(SensorType::SerialReceiver, idx_);
        if (proto::parseValueLine(line_.line(), v) && v >= d.validMin && v <= d.validMax)
          out[idx_].value = v;
        done = true;
        break;
      }
    }
    if (!done && (now - sentAt_).millis() < static_cast<int32_t>(board::features().serialTimeoutMs))
      return Step::Pending;
    idx_++; // Antwort da oder Zeitüberschreitung: nächster Kanal
    if (idx_ < n)
      request(idx_, now);
  }
  return Step::Ok;
}

} // namespace drivers
} // namespace ps

#endif
