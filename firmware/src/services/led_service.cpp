#include "services/led_service.h"

#include "services/log.h"

namespace ps {

void LedService::lights(ChannelStatus s, bool onlyRed, bool& g, bool& y, bool& r) {
  g = y = r = false;
  switch (s) {
  case ChannelStatus::Green:
    g = !onlyRed;
    break;
  case ChannelStatus::Yellow:
    y = !onlyRed;
    break;
  case ChannelStatus::Red:
    r = true;
    break;
  default:
    break;
  }
}

void LedService::tick(Instant now) {
  const DeviceConfig& d = cfg_.device();
  int slot = -1;
  if (d.ledMode == 2) {
    slot = schema::slotOfKey(d.ledChannel.c_str());
  } else if (d.ledMode == 1 && displayFn_) {
    slot = displayFn_(displayCtx_);
  }
  bool g = false, y = false, r = false;
  if (slot >= 0 && cfg_.sensors().channels[slot].enabled)
    lights(sensors_.status(static_cast<uint8_t>(slot), now), d.ledOnlyRed, g, y, r);
  const uint8_t bits = static_cast<uint8_t>((g ? 1 : 0) | (y ? 2 : 0) | (r ? 4 : 0));
  if (bits != lastBits_) {
    LOGD(Mod::Led, "LED: %s%s%s%s", g ? "grün " : "", y ? "gelb " : "", r ? "rot " : "",
         bits ? "" : "aus");
    lastBits_ = bits;
  }
  port_.set(g, y, r);
}

} // namespace ps
