/**
 * @file state_store.cpp
 */
#include "services/state_store.h"

#include "core/util/crc.h"
#include "hal/system.h"
#include "services/log.h"
#include "services/storage.h"

namespace ps {

namespace {
constexpr uint32_t kMagic = 0x54535350; // "PSST"
/// v2: zusätzlich Prüfsumme des Kanal-Layouts (Hardware aus dem Webflasher
/// kann sich ändern). v1 (feste Profile) wird gelesen, wenn die Kanalzahl passt.
/// v3: dahinter die Lerndaten der Analogkanäle (SoilFit), Anzahl im Kopf.
constexpr uint8_t kVersion = 3;
constexpr size_t kRecord = 4 + 2 + 4 + 4 + 2 + 2 + 4 + 4; // 26 Byte
constexpr size_t kFitRecord = SoilFit::kBins * (4 + 2);   // 60 Byte
constexpr size_t kHeaderV1 = 4 + 1 + 1 + 4 + 4;
constexpr size_t kHeaderV2 = kHeaderV1 + 4;
constexpr size_t kHeader = kHeaderV2 + 1;
constexpr size_t kMaxTotal =
    kHeader + board::kMaxSlots * kRecord + board::kMaxChannelsPerSensor * kFitRecord + 4;

size_t totalFor(uint8_t version, uint8_t slots, uint8_t fits) {
  if (version == 1)
    return kHeaderV1 + slots * kRecord + 4;
  if (version == 2)
    return kHeaderV2 + slots * kRecord + 4;
  return kHeader + slots * kRecord + fits * kFitRecord + 4;
}

/// Analogkanäle dieser Hardware (so viele SoilFit-Sätze)
uint8_t analogChannels() {
  const int a = board::sensorIndexOf(SensorType::Analog);
  return a < 0 ? 0 : board::sensor(a).channels;
}

/// Prüfsumme über Typ und Kanalindex jedes Slots
uint32_t layoutHash() {
  uint8_t b[board::kMaxSlots * 2];
  for (uint8_t s = 0; s < board::slotCount(); s++) {
    b[2 * s] = static_cast<uint8_t>(board::sensor(board::slot(s).sensor).type);
    b[2 * s + 1] = board::slot(s).channel;
  }
  return crc::crc32(b, board::slotCount() * 2u);
}

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
  for (auto& f : fit_)
    f.clear();
  mailLastWarning_ = mailLastAlive_ = 0;
  dirty_ = important_ = false;
}

bool StateStore::load() {
  hal::TempBuffer tb(kMaxTotal);
  if (!tb)
    return false;
  uint8_t* buf = tb.data();
  size_t len = 0;
  if (!storage::readBinary(fs_, kPath, buf, kMaxTotal, len))
    return false;
  if (len < kHeaderV1 + 4)
    return false;
  size_t at = 0;
  uint32_t magic;
  get(buf, at, &magic, 4);
  const uint8_t ver = buf[at++];
  const uint8_t count = buf[at++];
  if (magic != kMagic || ver < 1 || ver > kVersion)
    return false;
  const uint8_t fits = ver >= 3 && len > kHeader ? buf[kHeaderV2] : 0;
  if (len != totalFor(ver, count, fits)) {
    LOGW(Mod::Config, "state.bin hat falsche Größe (%u) - verworfen", static_cast<unsigned>(len));
    return false;
  }
  uint32_t stored;
  memcpy(&stored, buf + len - 4, 4);
  if (crc::crc32(buf, len - 4) != stored) {
    LOGW(Mod::Config, "state.bin Prüfsumme falsch - verworfen");
    return false;
  }
  if (count != board::slotCount())
    return false;
  get(buf, at, &mailLastWarning_, 4);
  get(buf, at, &mailLastAlive_, 4);
  if (ver >= 2) {
    uint32_t layout;
    get(buf, at, &layout, 4);
    if (layout != layoutHash()) {
      // Mail-Zeitpunkte gelten weiter, Kanalwerte gehören zu anderen Kanälen
      LOGI(Mod::Config, "Kanal-Layout geändert - gespeicherte Min/Max-Werte verworfen");
      return false;
    }
  }
  if (ver >= 3)
    at++; // Anzahl SoilFit, oben gelesen
  for (uint8_t i = 0; i < board::slotCount(); i++) {
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
  // Gleiches Layout heißt gleich viele Analogkanäle; sicher ist sicher
  for (uint8_t i = 0; i < fits && i < board::kMaxChannelsPerSensor; i++) {
    get(buf, at, fit_[i].raw, sizeof(fit_[i].raw));
    get(buf, at, fit_[i].n, sizeof(fit_[i].n));
  }
  return true;
}

bool StateStore::save() {
  hal::TempBuffer tb(kMaxTotal);
  if (!tb)
    return false;
  uint8_t* buf = tb.data();
  size_t at = 0;
  const uint32_t magic = kMagic;
  put(buf, at, &magic, 4);
  buf[at++] = kVersion;
  buf[at++] = board::slotCount();
  put(buf, at, &mailLastWarning_, 4);
  put(buf, at, &mailLastAlive_, 4);
  const uint32_t layout = layoutHash();
  put(buf, at, &layout, 4);
  const uint8_t fits = analogChannels();
  buf[at++] = fits;
  for (uint8_t i = 0; i < board::slotCount(); i++) {
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
  for (uint8_t i = 0; i < fits; i++) {
    put(buf, at, fit_[i].raw, sizeof(fit_[i].raw));
    put(buf, at, fit_[i].n, sizeof(fit_[i].n));
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
