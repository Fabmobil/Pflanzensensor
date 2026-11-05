/**
 * @file ota_service.h
 * @brief Firmware-Update im Normalbetrieb (refactor.md 2.12, 5.16)
 * @details Nur firmware.bin (E6). Vor dem Schreiben: Sensoren pausieren,
 *          Chronik/Log/Zustand flushen. Prüfungen: Größe, Magic-Byte, MD5
 *          (wenn angegeben bzw. verlangt), 60 s Leerlauf-Timeout. Bei Fehler
 *          bleibt die alte Firmware aktiv.
 */
#pragma once

#include "core/util/event_queue.h"
#include "hal/ota.h"
#include "services/config_service.h"

namespace ps {

class OtaService {
public:
  static constexpr uint32_t kIdleTimeoutMs = 60000;
  using PrepareFn = void (*)(void* ctx, bool starting);
  using ProgressFn = void (*)(void* ctx, uint8_t percent);

  OtaService(hal::Ota& ota, ConfigService& cfg, EventQueue<16>& events)
      : ota_(ota), cfg_(cfg), events_(events) {}
  void setHooks(PrepareFn prepare, ProgressFn progress, void* ctx) {
    prepare_ = prepare;
    progress_ = progress;
    ctx_ = ctx;
  }

  /// Upload beginnt; md5 darf leer sein (außer "Prüfsumme verlangen")
  Err start(uint32_t size, const char* md5, Instant now);
  void data(const uint8_t* d, size_t len, Instant now);
  /// Upload vollständig; true = neue Firmware gültig
  bool finish(Instant now);
  void abort(const char* why);
  /// Leerlauf-Timeout
  void tick(Instant now);

  bool active() const { return active_; }
  bool succeeded() const { return success_; }
  const char* error() const { return error_.c_str(); }
  void writeStatus(JsonWriter& w) const;

private:
  void fail(const char* why);
  hal::Ota& ota_;
  ConfigService& cfg_;
  EventQueue<16>& events_;
  PrepareFn prepare_ = nullptr;
  ProgressFn progress_ = nullptr;
  void* ctx_ = nullptr;
  bool active_ = false;
  bool success_ = false;
  bool failed_ = false;
  bool firstChunk_ = true;
  uint32_t size_ = 0;
  uint32_t written_ = 0;
  uint8_t lastPercent_ = 255;
  /// kleinster freier Heap während des Uploads (Abnahme O3: Ziel ≥ 8 KB)
  uint32_t minHeap_ = 0;
  Instant lastData_;
  FixedString<80> error_;
};

} // namespace ps
