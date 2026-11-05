/**
 * @file chronik_service.h
 * @brief Messwert-Historie: Recorder + Segmentspeicher (refactor.md 2.6, 5.8)
 * @details record() fasst nur RAM an; Flash wird ausschließlich in tick()
 *          (persist.tick) geschrieben. Der Kanalindex ist der stabile
 *          Kanalplatz (Slot 0..15); Slots ab 16 werden gezählt, aber nicht
 *          aufgezeichnet.
 */
#pragma once

#include "core/chronik/budget.h"
#include "core/chronik/format.h"
#include "core/chronik/segments.h"
#include "hal/fs.h"
#include "services/config_service.h"
#include "services/sensor_service.h"

namespace ps {

class ChronikService {
public:
  static constexpr size_t kBufferMax = 256;
  static constexpr size_t kFlushThreshold = 192;
  static constexpr uint32_t kFlushIntervalMs = 300000;
  /// Ohne Platz abgeschaltet: so oft das Budget neu prüfen
  static constexpr uint32_t kBudgetRecheckMs = 600000;

  ChronikService(hal::Fs& fs, ConfigService& cfg, SensorService& sensors)
      : fs_(fs), cfg_(cfg), sensors_(sensors) {}

  /// Segmente einlesen, jüngstes reparieren, Budget setzen
  void begin(bool disabledForBoot);
  /// Nach MeasurementDone (nur RAM)
  void record(uint8_t sensor, uint32_t epoch, Instant now);
  /// Einziger Flash-Schreibpfad; force: sofort (vor Neustart/OTA)
  void tick(Instant now, bool force);
  void requestBudgetRecheck() { budgetDirty_ = true; }

  /// Binärstrom ab Epoche (Segmente, aktuelle Tabelle, RAM-Puffer)
  bool stream(uint32_t since, Sink& out);
  /// CSV-Export (Speicher für den Dekoder wird kurz angefordert)
  bool exportCsv(Sink& out);

  bool enabled() const { return enabled_ && !bootDisabled_; }
  bool disabledForBoot() const { return bootDisabled_; }
  uint16_t segmentCount() const { return range_.count(); }
  uint8_t targetSegments() const { return target_; }
  uint32_t usedBytes() const;
  uint32_t dropped() const { return dropped_; }
  uint8_t skippedChannels() const;
  void writeInfo(JsonWriter& w) const;

private:
  void scan();
  void repairNewest();
  void enforceBudget();
  bool append();
  bool startSegment();
  /// Kanaltabelle in Rahmen ≤ 512 B; ruft fn je Rahmen
  bool writeTables(uint32_t epoch, bool (*fn)(void* ctx, const uint8_t* d, size_t n), void* ctx);
  uint32_t firstEpochOf(uint32_t index);
  void buildEncodings();

  hal::Fs& fs_;
  ConfigService& cfg_;
  SensorService& sensors_;
  chronik::SegmentRange range_;
  chronik::EncodingMap enc_;
  uint8_t buf_[kBufferMax];
  size_t bufLen_ = 0;
  Instant lastFlush_;
  Instant lastBudgetCheck_;
  uint32_t currentSize_ = 0;
  uint8_t target_ = 0;
  bool enabled_ = false;
  bool budgetDirty_ = true;
  bool bootDisabled_ = false;
  uint32_t dropped_ = 0;
  uint32_t lastDropLog_ = 0;
  bool dropLogged_ = false;
};

} // namespace ps
