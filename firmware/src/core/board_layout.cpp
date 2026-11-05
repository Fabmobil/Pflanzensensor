/**
 * @file board_layout.cpp
 * @brief Kanal-Layout aus der Hardware zur Laufzeit (siehe board.h)
 */
#include <string.h>

#include "board.h"

namespace board {

namespace {
struct Layout {
  ps::Hardware hw;
  SensorEntry sensors[kMaxSensors] = {};
  SlotEntry slots[kMaxSlots] = {};
  uint8_t sensorCount = 0;
  uint8_t slotCount = 0;
  bool init = false;
};

Layout g_layout;

void build(Layout& l, const ps::Hardware& hw) {
  l.hw = hw;
  l.sensorCount = 0;
  l.slotCount = 0;
  for (uint8_t i = 0; i < ps::kSensorTypeCount; i++) {
    const auto t = static_cast<ps::SensorType>(i);
    const uint8_t c = channelsOf(hw.features, t);
    if (c == 0)
      continue;
    l.sensors[l.sensorCount] = SensorEntry{t, l.slotCount, c};
    for (uint8_t ch = 0; ch < c; ch++)
      l.slots[l.slotCount++] = SlotEntry{l.sensorCount, ch};
    l.sensorCount++;
  }
  l.init = true;
}

Layout& layout() {
  if (!g_layout.init)
    build(g_layout, defaultHardware());
  return g_layout;
}
} // namespace

ps::Hardware defaultHardware() {
  ps::Hardware h;
  strncpy(h.name, board_profile::kName, sizeof(h.name) - 1);
  h.pins = board_profile::kPins;
  h.features = board_profile::kFeatures;
  h.staticIp = board_profile::kStaticIp;
  return h;
}

ps::HwCheckResult setHardware(const ps::Hardware& hw) {
  const ps::HwCheckResult r = ps::checkHardware(kPlatform, hw.features, hw.pins);
  if (r.problem != ps::HwProblem::None)
    return r;
  build(g_layout, hw);
  return r;
}

const ps::Hardware& hardware() { return layout().hw; }
uint8_t sensorCount() { return layout().sensorCount; }
uint8_t slotCount() { return layout().slotCount; }
const SensorEntry& sensor(uint8_t i) { return layout().sensors[i < kMaxSensors ? i : 0]; }
const SlotEntry& slot(uint8_t s) { return layout().slots[s < kMaxSlots ? s : 0]; }

int sensorIndexOf(ps::SensorType t) {
  Layout& l = layout();
  for (uint8_t i = 0; i < l.sensorCount; i++)
    if (l.sensors[i].type == t)
      return i;
  return -1;
}

} // namespace board

namespace ps {
const char* hwProblemCode(HwProblem p) {
  switch (p) {
  case HwProblem::None:
    return "ok";
  case HwProblem::DuplicatePin:
    return "duplicate_pin";
  case HwProblem::MissingPin:
    return "missing_pin";
  case HwProblem::NoSuchPin:
    return "no_such_pin";
  case HwProblem::FlashPin:
    return "flash_pin";
  case HwProblem::StrapPin:
    return "strap_pin";
  case HwProblem::InputOnlyPin:
    return "input_only_pin";
  case HwProblem::AdcPin:
    return "adc_pin";
  case HwProblem::AnalogChannels:
    return "analog_channels";
  case HwProblem::NeedMultiplexer:
    return "need_multiplexer";
  case HwProblem::Ds18b20Count:
    return "ds18b20_count";
  case HwProblem::Mhz19Range:
    return "mhz19_range";
  case HwProblem::NoSensor:
    return "no_sensor";
  case HwProblem::TooManyChannels:
    return "too_many_channels";
  }
  return "?";
}
} // namespace ps
