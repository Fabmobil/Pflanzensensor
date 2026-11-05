/**
 * @file log_service.cpp
 */
#include "services/log_service.h"

#include <stdio.h>

#include "hal/system.h"

namespace ps {

namespace log {
namespace {
Backend* g_backend = nullptr;

const char kModNames[static_cast<int>(Mod::Count)][8] PS_PROGMEM = {
    "Main", "Boot", "Config", "Sensor", "Zyklus",  "Display", "LED",    "WLAN",
    "Zeit", "Web",  "Auth",   "Mail",   "Chronik", "OTA",     "Speich", "FS"};
} // namespace

void attach(Backend* b) { g_backend = b; }

bool enabled(LogLevel level, Mod mod) {
  // Ohne Backend (früher Start, Tests): Warnungen und Fehler trotzdem zeigen
  if (!g_backend)
    return level >= LogLevel::Warning;
  return g_backend->enabled(level, mod);
}

void write(LogLevel level, Mod mod, const char* fmtP, ...) {
  va_list ap;
  va_start(ap, fmtP);
  if (g_backend) {
    g_backend->vwrite(level, mod, fmtP, ap);
  } else {
    char buf[LogService::kLineMax];
    ps_vsnprintf_P(buf, sizeof(buf), fmtP, ap);
    hal::consoleWrite(buf, strlen(buf));
    hal::consoleWrite("\n", 1);
  }
  va_end(ap);
}

FStr modName(Mod m) {
  const uint8_t i = static_cast<uint8_t>(m);
  return FStr{kModNames[i < static_cast<uint8_t>(Mod::Count) ? i : 0]};
}

DebugArea areaOf(Mod m) {
  switch (m) {
  case Mod::Mem:
    return DebugArea::Ram;
  case Mod::Cycle:
    return DebugArea::Cycle;
  case Mod::Sensor:
    return DebugArea::Sensor;
  case Mod::Display:
    return DebugArea::Display;
  case Mod::Web:
  case Mod::Auth:
    return DebugArea::Web;
  case Mod::Mail:
    return DebugArea::Mail;
  default:
    return DebugArea::None;
  }
}

} // namespace log

namespace {
const char kLevelTags[4][4] PS_PROGMEM = {".D.", ":I:", "!W!", "#E#"};
const char kLevelNames[4][8] PS_PROGMEM = {"DEBUG", "INFO", "WARNING", "ERROR"};

size_t formatTime(char* out, size_t cap, uint32_t ts, bool isEpoch) {
  hal::LocalTime lt;
  if (isEpoch && hal::toLocalTime(ts, lt)) {
    return static_cast<size_t>(snprintf(out, cap, "%04u-%02u-%02u %02u:%02u:%02u", lt.year,
                                        lt.month, lt.day, lt.hour, lt.minute, lt.second));
  }
  return static_cast<size_t>(snprintf(out, cap, "%lus", static_cast<unsigned long>(ts)));
}
} // namespace

void LogService::configure(const DeviceConfig& c) {
  level_ = static_cast<LogLevel>(c.logLevel > 3 ? 1 : c.logLevel);
  debugMask_ = 0;
  auto set = [&](bool on, DebugArea a) {
    if (on)
      debugMask_ |= static_cast<uint8_t>(1u << static_cast<uint8_t>(a));
  };
  set(c.debugRam, DebugArea::Ram);
  set(c.debugCycle, DebugArea::Cycle);
  set(c.debugSensor, DebugArea::Sensor);
  set(c.debugDisplay, DebugArea::Display);
  set(c.debugWeb, DebugArea::Web);
  set(c.debugMail, DebugArea::Mail);
  fileEnabled_ = c.fileLog;
}

bool LogService::enabled(LogLevel level, Mod mod) const {
  // Das Level ist eine harte Schwelle. Die Debug-Schalter wählen nur bei
  // Level DEBUG aus, welche Bereiche reden; ist keiner an, reden alle.
  // Module ohne Bereich (Boot, WLAN, Zeit, …) zeigen ihre Debug-Zeilen immer.
  if (level != LogLevel::Debug)
    return level >= level_;
  if (level_ != LogLevel::Debug)
    return false;
  if (debugMask_ == 0)
    return true;
  const DebugArea a = log::areaOf(mod);
  return a == DebugArea::None || (debugMask_ & (1u << static_cast<uint8_t>(a)));
}

void LogService::vwrite(LogLevel level, Mod mod, const char* fmtP, va_list ap) {
  if (inWrite_)
    return; // keine Rekursion aus Ausgabepfaden
  inWrite_ = true;
  char text[kLineMax];
  const int n = ps_vsnprintf_P(text, sizeof(text), fmtP, ap);
  size_t len = n < 0 ? 0 : static_cast<size_t>(n);
  if (len >= sizeof(text)) {
    // Überlänge mit "…" (3 Byte UTF-8) markieren, nie mitten im Zeichen
    len = utf8Truncate(text, sizeof(text) - 1, sizeof(text) - 4);
    memcpy(text + len, "\xE2\x80\xA6", 3);
    len += 3;
    text[len] = '\0';
  }
  const uint32_t ep = hal::epoch();
  const bool isEpoch = epochValid(ep);
  const uint32_t ts = isEpoch ? ep : hal::now().millis() / 1000;

  char line[kLineMax + 48];
  size_t at = formatTime(line, sizeof(line), ts, isEpoch);
  char tag[4];
  copyP(tag, sizeof(tag), FStr{kLevelTags[static_cast<uint8_t>(level) & 3]});
  char mod_[8];
  copyP(mod_, sizeof(mod_), log::modName(mod));
  at += static_cast<size_t>(snprintf(line + at, sizeof(line) - at, " %s [%s] ", tag, mod_));
  const size_t room = sizeof(line) - at - 2;
  const size_t take = len < room ? len : room;
  memcpy(line + at, text, take);
  at += take;
  line[at++] = '\n';
  line[at] = '\0';

  hal::consoleWrite(line, at);
  pushRing(level, mod, ts, isEpoch, text, len);
  if (fileEnabled_ && !fileBlocked_)
    pushFile(line, at);
  inWrite_ = false;
}

void LogService::pushRing(LogLevel level, Mod mod, uint32_t ts, bool tsIsEpoch, const char* text,
                          size_t len) {
  if (len > 255)
    len = 255;
  const size_t recLen = 11 + len;
  // Platz schaffen: älteste Datensätze verwerfen
  while (kRingSize - ringUsed_ < recLen && ringUsed_ > 0) {
    const uint8_t oldLen = ring_[(ringHead_ + 10) % kRingSize];
    const size_t drop = 11 + oldLen;
    ringHead_ = (ringHead_ + drop) % kRingSize;
    ringUsed_ -= drop;
    oldestSeq_++;
  }
  uint8_t hdr[11];
  const uint32_t s = seq_;
  for (int i = 0; i < 4; i++)
    hdr[i] = static_cast<uint8_t>(s >> (8 * i));
  for (int i = 0; i < 4; i++)
    hdr[4 + i] = static_cast<uint8_t>(ts >> (8 * i));
  hdr[8] = static_cast<uint8_t>(static_cast<uint8_t>(level) | (tsIsEpoch ? 4 : 0));
  hdr[9] = static_cast<uint8_t>(mod);
  hdr[10] = static_cast<uint8_t>(len);
  size_t pos = (ringHead_ + ringUsed_) % kRingSize;
  for (size_t i = 0; i < 11; i++) {
    ring_[pos] = hdr[i];
    pos = (pos + 1) % kRingSize;
  }
  for (size_t i = 0; i < len; i++) {
    ring_[pos] = static_cast<uint8_t>(text[i]);
    pos = (pos + 1) % kRingSize;
  }
  ringUsed_ += recLen;
  if (ringUsed_ == recLen)
    oldestSeq_ = s;
  seq_++;
}

void LogService::writeSince(JsonWriter& w, uint32_t since, uint16_t maxLines) const {
  w.beginObject();
  uint32_t next = since;
  uint32_t dropped = 0;
  if (ringUsed_ == 0) {
    if (since < seq_)
      dropped = seq_ - since;
    next = seq_;
  } else if (since < oldestSeq_) {
    dropped = oldestSeq_ - since;
    next = oldestSeq_;
  }
  if (since > seq_)
    next = seq_; // Gerät neu gestartet: von vorn
  w.key(FS_("lines")).beginArray();
  size_t pos = ringHead_;
  size_t left = ringUsed_;
  uint16_t lines = 0;
  while (left >= 11 && lines < maxLines) {
    uint8_t hdr[11];
    for (size_t i = 0; i < 11; i++)
      hdr[i] = ring_[(pos + i) % kRingSize];
    const uint32_t s = uint32_t(hdr[0]) | (uint32_t(hdr[1]) << 8) | (uint32_t(hdr[2]) << 16) |
                       (uint32_t(hdr[3]) << 24);
    const uint32_t ts = uint32_t(hdr[4]) | (uint32_t(hdr[5]) << 8) | (uint32_t(hdr[6]) << 16) |
                        (uint32_t(hdr[7]) << 24);
    const uint8_t len = hdr[10];
    if (s >= next || (since > seq_)) {
      char text[256];
      for (size_t i = 0; i < len; i++)
        text[i] = static_cast<char>(ring_[(pos + 11 + i) % kRingSize]);
      char tbuf[24];
      formatTime(tbuf, sizeof(tbuf), ts, (hdr[8] & 4) != 0);
      char lv[8];
      copyP(lv, sizeof(lv), FStr{kLevelNames[hdr[8] & 3]});
      w.beginObject();
      w.kv(FS_("seq"), s);
      w.kv(FS_("ts"), static_cast<const char*>(tbuf));
      w.kv(FS_("level"), static_cast<const char*>(lv));
      w.key(FS_("module")).value(log::modName(static_cast<Mod>(hdr[9])));
      w.key(FS_("text")).value(text, len);
      w.endObject();
      next = s + 1;
      lines++;
    }
    pos = (pos + 11 + len) % kRingSize;
    left -= 11 + len;
  }
  w.endArray();
  w.kv(FS_("next"), next);
  w.kv(FS_("dropped"), dropped);
  w.endObject();
}

void LogService::pushFile(const char* line, size_t len) {
  if (fileLen_ + len > kFileBuffer) {
    fileDropped_++;
    return;
  }
  if (fileLen_ == 0)
    fileFirstAt_ = hal::now().millis();
  memcpy(fileBuf_ + fileLen_, line, len);
  fileLen_ += len;
}

void LogService::flush(bool force) {
  if (!fileEnabled_ || fileBlocked_) {
    fileLen_ = 0;
    fileDropped_ = 0;
    return;
  }
  if (fileLen_ == 0 && fileDropped_ == 0)
    return;
  const bool due = fileLen_ >= kFileFlushAt || (hal::now().millis() - fileFirstAt_) >= kFileFlushMs;
  if (!force && !due)
    return;
  rotateIfNeeded();
  hal::FileGuard f(fs_, fs_.open("/log.txt", hal::OpenMode::Append));
  if (!f) {
    fileLen_ = 0;
    return;
  }
  size_t written = f->writeBytes(reinterpret_cast<const uint8_t*>(fileBuf_), fileLen_);
  if (fileDropped_) {
    char msg[48];
    const int n = snprintf(msg, sizeof(msg), "--- %lu Logzeilen verworfen ---\n",
                           static_cast<unsigned long>(fileDropped_));
    written += f->writeBytes(reinterpret_cast<const uint8_t*>(msg), static_cast<size_t>(n));
    fileDropped_ = 0;
  }
  if (knownSize_ >= 0)
    knownSize_ += static_cast<int32_t>(written);
  fileLen_ = 0;
}

void LogService::rotateIfNeeded() {
  if (knownSize_ < 0) {
    hal::FileGuard f(fs_, fs_.open("/log.txt", hal::OpenMode::Read));
    knownSize_ = f ? static_cast<int32_t>(f->size()) : 0;
  }
  const uint32_t size = static_cast<uint32_t>(knownSize_);
  if (size + fileLen_ <= kMaxFileSize)
    return;
  // Neuere Hälfte behalten: ab Mitte bis zum nächsten Zeilenende überspringen
  hal::FileGuard in(fs_, fs_.open("/log.txt", hal::OpenMode::Read));
  hal::FileGuard out(fs_, fs_.open("/log.txt.tmp", hal::OpenMode::Write));
  if (!in || !out) {
    knownSize_ = -1;
    return;
  }
  in->seek(size / 2);
  // Großer Puffer aus dem Temp-Speicher: weniger LittleFS-Aufrufe
  constexpr size_t kBig = 1024;
  uint8_t small[128];
  uint8_t* buf = static_cast<uint8_t*>(hal::tempAlloc(kBig));
  const size_t cap = buf ? kBig : sizeof(small);
  if (!buf)
    buf = small;
  bool lineStart = false;
  uint32_t kept = 0;
  for (;;) {
    const size_t n = in->read(buf, cap);
    if (n == 0)
      break;
    size_t from = 0;
    if (!lineStart) {
      while (from < n && buf[from] != '\n')
        from++;
      if (from < n) {
        from++;
        lineStart = true;
      } else {
        continue;
      }
    }
    if (n > from)
      kept += static_cast<uint32_t>(out->writeBytes(buf + from, n - from));
  }
  if (buf != small)
    hal::tempFree(buf);
  in.reset();
  out.reset();
  knownSize_ = fs_.rename("/log.txt.tmp", "/log.txt") ? static_cast<int32_t>(kept) : -1;
}

void LogService::deleteFile() {
  knownSize_ = -1;
  fs_.remove("/log.txt");
  fs_.remove("/log.txt.tmp");
}

uint32_t LogService::fileSize() {
  if (knownSize_ >= 0)
    return static_cast<uint32_t>(knownSize_);
  hal::FileGuard f(fs_, fs_.open("/log.txt", hal::OpenMode::Read));
  return f ? f->size() : 0;
}

} // namespace ps
