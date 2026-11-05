/**
 * @file sds011.cpp
 */
#if defined(ARDUINO)

#include "drivers/sds011.h"

#include "board.h"
#include "platform/arduino/uart_port.h"

namespace ps {
namespace drivers {

namespace {
platform::UartPort& port() {
  static platform::UartPort p(1);
  return p;
}
} // namespace

void Sds011::send(const uint8_t* cmd) { port().write(cmd, 19); }

Step Sds011::begin(Instant) {
  if (!begun_)
    begun_ = port().begin(9600, board::kPins.sdsRx, board::kPins.sdsTx);
  return begun_ ? Step::Ok : Step::Error;
}

Step Sds011::wake(Instant) {
  uint8_t cmd[19];
  proto::sdsWork(true, cmd);
  send(cmd);
  return Step::Ok;
}

Step Sds011::startRound(Instant) {
  port().flushInput();
  parser_.reset();
  uint8_t cmd[19];
  proto::sdsQuery(cmd);
  send(cmd);
  return Step::Ok;
}

Step Sds011::pollRound(Instant, Reading* out, uint8_t n) {
  while (port().available() > 0) {
    if (parser_.feed(static_cast<uint8_t>(port().read()))) {
      if (n > 0 && parser_.pm10() <= 999.9f)
        out[0].value = parser_.pm10();
      if (n > 1 && parser_.pm25() <= 999.9f)
        out[1].value = parser_.pm25();
      return Step::Ok;
    }
  }
  return Step::Pending; // Frist der Runde begrenzt das Warten
}

void Sds011::sleep() {
  uint8_t cmd[19];
  proto::sdsWork(false, cmd);
  send(cmd);
}

} // namespace drivers
} // namespace ps

#endif
