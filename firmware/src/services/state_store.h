/**
 * @file state_store.h
 * @brief Laufzeitzustand der Kanäle und Mail-Zeitstempel, gedrosselt gespeichert
 * @details /state.bin: binär mit Version und CRC32. Geschrieben wird
 *          höchstens alle 15 min, bei neuen Extremwerten frühestens nach 5 min
 *          und vor Neustart/OTA (refactor.md 5.7). Die Write-Behind-Queue des
 *          Altcodes, die jede Messung in jede Kanaldatei schrieb (B-15), gibt
 *          es nicht mehr.
 */
#pragma once

#include "board.h"
#include "core/sensors/channel.h"
#include "hal/fs.h"

namespace ps {

class StateStore {
public:
  static constexpr const char* kPath = "/state.bin";
  static constexpr uint32_t kSaveIntervalMs = 15UL * 60 * 1000;
  static constexpr uint32_t kExtremaDelayMs = 5UL * 60 * 1000;

  explicit StateStore(hal::Fs& fs) : fs_(fs) {}

  bool load();
  /// Aus persist.tick; force: sofort (vor Neustart/OTA)
  void tick(Instant now, bool force);

  ChannelState& channel(uint8_t slot) { return ch_[slot]; }
  const ChannelState& channel(uint8_t slot) const { return ch_[slot]; }

  /// Wert geändert (normaler Takt) bzw. Extremwerte/Kalibrierung geändert
  void markDirty(Instant now) {
    if (!dirty_)
      dirtySince_ = now;
    dirty_ = true;
  }
  void markImportant(Instant now) {
    markDirty(now);
    if (!important_)
      importantSince_ = now;
    important_ = true;
  }

  uint32_t mailLastWarning() const { return mailLastWarning_; }
  uint32_t mailLastAlive() const { return mailLastAlive_; }
  void setMailTimes(uint32_t warning, uint32_t alive, Instant now) {
    mailLastWarning_ = warning;
    mailLastAlive_ = alive;
    markImportant(now);
    importantSince_ = now - Duration::ms(kExtremaDelayMs); // sofort fällig
  }

  void clear();
  bool save();

private:
  hal::Fs& fs_;
  ChannelState ch_[board::kSlotCount];
  uint32_t mailLastWarning_ = 0;
  uint32_t mailLastAlive_ = 0;
  bool dirty_ = false;
  bool important_ = false;
  Instant dirtySince_;
  Instant importantSince_;
  Instant lastSave_;
};

} // namespace ps
