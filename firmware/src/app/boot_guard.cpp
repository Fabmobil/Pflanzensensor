/**
 * @file boot_guard.cpp
 */
#include "app/boot_guard.h"

#include "hal/rtc_mem.h"
#include "services/log.h"

namespace ps {

namespace {
constexpr uint32_t kMagic = 0x50534247; // "PSBG"
}

uint8_t BootGuard::nextCount(uint8_t stored, bool valid, hal::ResetReason reason) {
  if (!valid || reason == hal::ResetReason::PowerOn || reason == hal::ResetReason::Brownout)
    return 1;
  return stored < 250 ? static_cast<uint8_t>(stored + 1) : stored;
}

void BootGuard::begin(hal::ResetReason reason) {
  Rtc r{};
  const bool valid = hal::rtcRead(&r, sizeof(r)) && r.magic == kMagic && r.check == checksum(r);
  st_.magic = kMagic;
  st_.bootCount = nextCount(valid ? r.bootCount : 0, valid, reason);
  st_.sensorRestarts = valid && reason != hal::ResetReason::PowerOn ? r.sensorRestarts : 0;
  save();
  if (st_.bootCount >= 2)
    LOGW(Mod::Boot, "Schneller Neustart Nr. %u erkannt", st_.bootCount);
}

void BootGuard::save() {
  st_.check = checksum(st_);
  hal::rtcWrite(&st_, sizeof(st_));
}

void BootGuard::tick(Instant now) {
  if (!bootCleared_ && now.millis() >= kStableMs) {
    bootCleared_ = true;
    if (st_.bootCount != 0) {
      st_.bootCount = 0;
      save();
      LOGD(Mod::Boot, "Stabile Laufzeit - Startzähler gelöscht");
    }
  }
  if (!sensorCleared_ && now.millis() >= kSensorStableMs) {
    sensorCleared_ = true;
    if (st_.sensorRestarts != 0) {
      st_.sensorRestarts = 0;
      save();
    }
  }
}

bool BootGuard::allowSensorRestart() {
  if (st_.sensorRestarts >= kMaxSensorRestarts) {
    LOGW(Mod::Boot, "Sensor-Neustart verweigert (schon %u in Folge)", st_.sensorRestarts);
    return false;
  }
  st_.sensorRestarts++;
  save();
  return true;
}

} // namespace ps
