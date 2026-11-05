/**
 * @file system_info.h
 * @brief Heap, Laufzeit, Neustarts, Dateisystem (gecacht)
 * @details Der Neustartzähler steht in /boot.cnt und wird einmal pro Boot
 *          gelesen und erhöht, danach nur noch aus dem RAM geliefert (B-61).
 */
#pragma once

#include "core/json_writer.h"
#include "hal/fs.h"
#include "hal/system.h"

namespace ps {

class SystemInfo {
public:
  static constexpr const char* kBootCountPath = "/boot.cnt";
  static constexpr uint32_t kCriticalHeap = 6000;

  explicit SystemInfo(hal::Fs& fs) : fs_(fs) {}

  void begin();
  /// Alle 30 s
  void tick(Instant now);
  /// Aus persist.tick: Neustartzähler einmalig schreiben
  void persist();

  uint32_t rebootCount() const { return reboots_; }
  uint32_t uptimeS() const { return hal::now().millis() / 1000 + wraps_ * 4294967UL; }
  const hal::HeapInfo& heap() const { return heap_; }
  uint32_t minFreeHeap() const { return minFree_; }
  /// Liest frisch: LittleFS durchläuft dafür alle Blöcke (~80 ms), deshalb
  /// nur auf Anfrage und nicht im 30-s-Takt
  const hal::FsInfo& fsInfo() {
    fs_.info(fs_info_);
    return fs_info_;
  }
  /// Eindeutige Kennung dieses Starts (für DeviceWait)
  uint32_t bootId() const { return bootId_; }

  void writeHeap(JsonWriter& w) const;

private:
  hal::Fs& fs_;
  uint32_t reboots_ = 0;
  bool dirty_ = false;
  hal::HeapInfo heap_;
  uint32_t minFree_ = UINT32_MAX;
  hal::FsInfo fs_info_;
  uint32_t lastMs_ = 0;
  uint32_t wraps_ = 0;
  uint32_t bootId_ = 0;
};

} // namespace ps
