/**
 * @file log_service.h
 * @brief Logger mit Serial, Datei-Puffer und Live-Ring (refactor.md 2.10, 5.10)
 * @details Der Live-Ring (2 KB) ersetzt den WebSocket: Die Seite /logs pollt
 *          GET /api/v1/logs?since=<seq>. Nichts wird synchron aus dem
 *          Log-Aufruf über das Netz gesendet (B-46), es gibt kein 256-B-Limit
 *          (B-43) und beliebig viele Betrachter.
 *
 *          Datei-Log /log.txt: RAM-Puffer 1 KB, Flush ab 512 B oder nach 5 s,
 *          ausschließlich aus persist.tick. Rotation bei 16000 B: die neuere
 *          Hälfte bleibt.
 */
#pragma once

#include "hal/fs.h"
#include "services/log.h"

namespace ps {

class LogService : public log::Backend {
public:
  static constexpr size_t kLineMax = 160;
  static constexpr size_t kRingSize = 2048;
  static constexpr size_t kFileBuffer = 1024;
  static constexpr size_t kFileFlushAt = 512;
  static constexpr uint32_t kFileFlushMs = 5000;
  static constexpr uint32_t kMaxFileSize = 16000;

  explicit LogService(hal::Fs& fs) : fs_(fs) {}

  void configure(const DeviceConfig& c);
  void setLevel(LogLevel l) { level_ = l; }
  LogLevel level() const { return level_; }
  /// Datei-Log für diesen Start sperren (Boot-Schleife)
  void blockFileLog() { fileBlocked_ = true; }
  bool fileLogActive() const { return fileEnabled_ && !fileBlocked_; }

  bool enabled(LogLevel level, Mod mod) const override;
  void vwrite(LogLevel level, Mod mod, const char* fmtP, va_list ap) override;

  /// Aus persist.tick: Datei-Puffer schreiben, wenn fällig (force: sofort)
  void flush(bool force);
  /// Datei löschen (Boot-Schleife, Werksreset bleibt aber ohne)
  void deleteFile();
  uint32_t fileSize();

  /// Live-Ring als JSON: {"next":n,"dropped":d,"lines":[…]}
  void writeSince(JsonWriter& w, uint32_t since, uint16_t maxLines = 60) const;
  uint32_t nextSeq() const { return seq_; }

private:
  void format(char* out, size_t cap, LogLevel level, Mod mod, const char* text, size_t& prefixLen);
  void pushRing(LogLevel level, Mod mod, uint32_t ts, bool tsIsEpoch, const char* text, size_t len);
  void pushFile(const char* line, size_t len);
  void rotateIfNeeded();

  hal::Fs& fs_;
  LogLevel level_ = LogLevel::Info;
  uint8_t debugMask_ = 0; ///< Bit je DebugArea
  bool fileEnabled_ = false;
  bool fileBlocked_ = false;

  // Live-Ring: Datensätze {u32 seq, u32 ts, u8 flags(level|epoch<<2), u8 mod, u8 len, text}
  uint8_t ring_[kRingSize];
  size_t ringHead_ = 0; ///< ältester Datensatz
  size_t ringUsed_ = 0;
  uint32_t seq_ = 0; ///< nächste Sequenznummer
  uint32_t oldestSeq_ = 0;

  char fileBuf_[kFileBuffer];
  size_t fileLen_ = 0;
  uint32_t fileDropped_ = 0;
  uint32_t fileFirstAt_ = 0;
  /// Größe von /log.txt, -1 = unbekannt (spart je Flush ein open/close)
  int32_t knownSize_ = -1;
  bool inWrite_ = false;
};

} // namespace ps
