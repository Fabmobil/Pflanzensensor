#include "core/sensors/status.h"

namespace ps {

namespace {
const char kNames[6][8] PS_PROGMEM = {"green", "yellow", "red", "error", "unknown", "warmup"};
}

FStr statusName(ChannelStatus s) {
  const uint8_t i = static_cast<uint8_t>(s);
  return FStr{kNames[i < 6 ? i : 4]};
}

ChannelStatus statusFromName(const char* s) {
  if (!s)
    return ChannelStatus::Unknown;
  for (uint8_t i = 0; i < 6; i++)
    if (ps_strcmp_P(s, kNames[i]) == 0)
      return static_cast<ChannelStatus>(i);
  return ChannelStatus::Unknown;
}

} // namespace ps
