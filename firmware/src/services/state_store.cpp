/**
 * @file state_store.cpp
 */
#include "services/state_store.h"

#include "core/util/crc.h"
#include "services/log.h"
#include "services/storage.h"

namespace ps {

namespace {
constexpr uint32_t kMagic = 0x54535350; // "PSST"
constexpr uint8_t kVersion = 1;
constexpr size_t kRecord = 4 + 2 + 4 + 4 + 2 + 2 + 4 + 4; // 26 Byte
constexpr size_t kHeader = 4 + 1 + 1 + 4 + 4;
constexpr size_t kTotal = kHeader + board::kSlotCount * kRecord + 4;

void put(uint8_t* b, size_t& at, const void* v, size_t n) {
  memcpy(b + at, v, n); // Plattformen sind little-endian
  at += n;
}
void get(const uint8_t* b, size_t& at, void* v, size_t n) {
  memcpy(v, b + at, n);
  at += n;
}
} // namespace

void StateStore::clear() {
  for (auto& c : ch_)
    c = ChannelState{};
  mailLastWarning_ = mailLastAlive_ = 0;
  dirty_ = important_ = false;
}

bool StateStore::load() {
  uint8_t buf[kTotal];
  size_t len = 0;
  if (!storage::readBinary(fs_, kPath, buf, sizeof(buf), len))
    return false;
  if (len != kTotal) {
    LOGW(Mod::Config, "state.bin hat falsche Größe (%u) - verworfen", static_cast<unsigned>(len));
    return false;
  }
  uint32_t stored;
  memcpy(&stored, buf + kTotal - 4, 4);
  if (crc::crc32(buf, kTotal - 4) != stored) {
    LOGW(Mod::Config, "state.bin Prüfsumme falsch - verworfen");
    return false;
  }
  size_t at = 0;
  uint32_t magic;
  get(buf, at, &magic, 4);
  const uint8_t ver = buf[at++];
  const uint8_t count = buf[at++];
  if (magic != kMagic || ver != kVersion || count != board::kSlotCount)
    return false;
  get(buf, at, &mailLastWarning_, 4);
  get(buf, at, &mailLastAlive_, 4);
  for (uint8_t i = 0; i < board::kSlotCount; i++) {
    ChannelState& c = ch_[i];
    get(buf, at, &c.value, 4);
    get(buf, at, &c.raw, 2);
    get(buf, at, &c.obsMin, 4);
    get(buf, at, &c.obsMax, 4);
    get(buf, at, &c.rawMin, 2);
    get(buf, at, &c.rawMax, 2);
    get(buf, at, &c.cal.min, 4);
    get(buf, at, &c.cal.max, 4);
  }
  return true;
}

bool StateStore::save() {
  uint8_t buf[kTotal];
  size_t at = 0;
  const uint32_t magic = kMagic;
  put(buf, at, &magic, 4);
  buf[at++] = kVersion;
  buf[at++] = board::kSlotCount;
  put(buf, at, &mailLastWarning_, 4);
  put(buf, at, &mailLastAlive_, 4);
  for (uint8_t i = 0; i < board::kSlotCount; i++) {
    const ChannelState& c = ch_[i];
    put(buf, at, &c.value, 4);
    put(buf, at, &c.raw, 2);
    put(buf, at, &c.obsMin, 4);
    put(buf, at, &c.obsMax, 4);
    put(buf, at, &c.rawMin, 2);
    put(buf, at, &c.rawMax, 2);
    put(buf, at, &c.cal.min, 4);
    put(buf, at, &c.cal.max, 4);
  }
  const uint32_t c = crc::crc32(buf, at);
  put(buf, at, &c, 4);
  struct Ctx {
    const uint8_t* b;
    size_t n;
  } ctx{buf, at};
  return storage::writeAtomic(
      fs_, kPath,
      [](hal::File& f, void* p) {
        Ctx& x = *static_cast<Ctx*>(p);
        return f.writeBytes(x.b, x.n) == x.n;
      },
      &ctx);
}

void StateStore::tick(Instant now, bool force) {
  if (!dirty_)
    return;
  bool due = force || (now - lastSave_).millis() >= static_cast<int32_t>(kSaveIntervalMs);
  if (important_ && (now - importantSince_).millis() >= static_cast<int32_t>(kExtremaDelayMs))
    due = true;
  if (!due)
    return;
  if (save()) {
    dirty_ = important_ = false;
    lastSave_ = now;
    LOGD(Mod::Mem, "Kanalzustand gespeichert");
  } else {
    lastSave_ = now; // nicht in jeder Sekunde erneut versuchen
  }
}

} // namespace ps
