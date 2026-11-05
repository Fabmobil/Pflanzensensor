/**
 * @file chronik_service.cpp
 */
#include "services/chronik_service.h"

#include <new>

#include "core/chronik/csv.h"
#include "hal/system.h"
#include "services/log.h"

namespace ps {

using namespace chronik;

namespace {
constexpr size_t kChunk = 256;

uint8_t statusCode(ChannelStatus s) {
  switch (s) {
  case ChannelStatus::Green:
    return kGreen;
  case ChannelStatus::Yellow:
    return kYellow;
  case ChannelStatus::Red:
    return kRed;
  case ChannelStatus::Error:
    return kError;
  case ChannelStatus::Warmup:
    return kWarmup;
  default:
    return kUnknown;
  }
}
} // namespace

void ChronikService::buildEncodings() {
  for (uint8_t s = 0; s < board::kSlotCount && s < kMaxChannels; s++) {
    const auto& sl = board::kSlots[s];
    const ChannelDef d = catalog::channel(board::kSensors[sl.sensor].type, sl.channel);
    enc_.enc[s].kind = d.encoding;
    enc_.enc[s].scale = d.encScale;
    enc_.enc[s].offset = d.encOffset;
    enc_.known[s] = true;
  }
}

void ChronikService::begin(bool disabledForBoot) {
  lastFlush_ = hal::now();
  buildEncodings();
  if (disabledForBoot) {
    LOGW(Mod::Chronik, "Mehrfachstart erkannt - Chronik bleibt für diesen Start aus");
    bootDisabled_ = true;
    enabled_ = false;
    budgetDirty_ = false;
    return;
  }
  scan();
  repairNewest();
  enforceBudget();
  LOGI(Mod::Chronik, "Bereit: %u von %u Segmenten, %lu KB belegt", range_.count(), target_,
       static_cast<unsigned long>(usedBytes() / 1024));
}

void ChronikService::scan() {
  range_.reset();
  currentSize_ = 0;
  struct Ctx {
    ChronikService* self;
  } ctx{this};
  fs_.list(
      [](void* c, const char* name, uint32_t size) {
        ChronikService* self = static_cast<Ctx*>(c)->self;
        const int64_t idx = indexFromName(name);
        if (idx < 0)
          return;
        self->range_.add(static_cast<uint32_t>(idx));
        if (self->range_.newest() == static_cast<uint32_t>(idx))
          self->currentSize_ = size;
      },
      &ctx);
  // currentSize_ gehört zum jüngsten; bei unsortierter Liste nachmessen
  if (!range_.empty()) {
    char name[kNameBuffer];
    nameFromIndex(range_.newest(), name);
    hal::FileGuard f(fs_, fs_.open(name, hal::OpenMode::Read));
    currentSize_ = f ? f->size() : 0;
  }
}

void ChronikService::repairNewest() {
  if (range_.empty())
    return;
  char name[kNameBuffer];
  nameFromIndex(range_.newest(), name);
  uint32_t size = 0, valid = 0;
  {
    hal::FileGuard f(fs_, fs_.open(name, hal::OpenMode::Read));
    if (!f)
      return;
    size = f->size();
    uint8_t* window = static_cast<uint8_t*>(hal::tempAlloc(kMaxFrame));
    if (!window)
      return;
    while (valid < size) {
      if (!f->seek(valid))
        break;
      const size_t n = f->read(window, kMaxFrame);
      if (!n)
        break;
      const size_t len = frameLength(window, n);
      if (!len)
        break;
      valid += len;
      hal::yieldNow();
    }
    hal::tempFree(window);
  }
  if (valid == size)
    return;
  LOGW(Mod::Chronik, "Angeschnittener Rahmen in %s: kürze von %lu auf %lu Bytes", name,
       static_cast<unsigned long>(size), static_cast<unsigned long>(valid));
  hal::FileGuard f(fs_, fs_.open(name, hal::OpenMode::ReadWrite));
  if (f && f->truncate(valid))
    currentSize_ = valid;
}

uint32_t ChronikService::usedBytes() const {
  if (range_.empty())
    return 0;
  return static_cast<uint32_t>(range_.count() - 1) * kSegmentSize + currentSize_;
}

uint8_t ChronikService::skippedChannels() const {
  uint8_t n = 0;
  for (uint8_t s = kMaxChannels; s < board::kSlotCount; s++)
    if (cfg_.sensors().channels[s].enabled)
      n++;
  return n;
}

void ChronikService::enforceBudget() {
  lastBudgetCheck_ = hal::now();
  hal::FsInfo info;
  if (!fs_.info(info)) {
    LOGE(Mod::Chronik, "Dateisysteminformationen nicht lesbar");
    return;
  }
  BudgetInput in;
  in.freeBytes = info.total - info.used;
  in.ownBytes = static_cast<uint32_t>(range_.count()) * chronik::kSegmentFootprint;
  in.fileLogEnabled = cfg_.device().fileLog;
  const uint8_t t = chronik::targetSegments(in);
  if (t != target_) {
    if (t == 0)
      LOGW(Mod::Chronik, "Chronik aus: zu wenig Flash frei (%lu KB)",
           static_cast<unsigned long>(in.freeBytes / 1024));
    else
      LOGI(Mod::Chronik, "Budget: %u Segmente (frei %lu KB)", t,
           static_cast<unsigned long>(in.freeBytes / 1024));
  }
  target_ = t;
  budgetDirty_ = false;
  uint8_t excess =
      excessSegments(range_.count() > 255 ? 255 : static_cast<uint8_t>(range_.count()), target_);
  while (excess > 0 && range_.count() > 0) {
    char name[kNameBuffer];
    nameFromIndex(range_.oldest(), name);
    if (!fs_.remove(name)) {
      LOGE(Mod::Chronik, "Ältestes Segment nicht löschbar: %s", name);
      break;
    }
    scan();
    excess--;
  }
  enabled_ = target_ > 0;
}

void ChronikService::record(uint8_t sensor, uint32_t epoch, Instant now) {
  if (bootDisabled_ || !enabled_)
    return;
  if (epoch < kMinEpoch) {
    dropped_++;
    if (!dropLogged_ || now.millis() - lastDropLog_ > 3600000UL) {
      dropLogged_ = true;
      lastDropLog_ = now.millis();
      LOGW(Mod::Chronik, "Keine synchronisierte Zeit - %lu Rahmen verworfen",
           static_cast<unsigned long>(dropped_));
    }
    return;
  }
  SampleFrame f;
  f.epoch = epoch;
  const auto& se = board::kSensors[sensor];
  for (uint8_t ch = 0; ch < se.channels; ch++) {
    const uint8_t slot = static_cast<uint8_t>(se.firstSlot + ch);
    if (slot >= kMaxChannels || !cfg_.sensors().channels[slot].enabled)
      continue;
    const ChannelState& st = sensors_.channel(slot);
    ChannelValue& v = f.values[f.count];
    v.channel = slot;
    v.status = statusCode(st.status);
    v.value = st.value;
    if (catalog::channel(se.type, ch).hasRaw && st.raw >= 0) {
      v.hasRaw = true;
      v.raw = st.raw;
    }
    f.count++;
  }
  if (!f.count)
    return;
  const size_t n = writeSample(f, enc_, buf_ + bufLen_, kBufferMax - bufLen_);
  if (!n) {
    dropped_++;
    return;
  }
  bufLen_ += n;
}

void ChronikService::tick(Instant now, bool force) {
  if (bootDisabled_) {
    bufLen_ = 0;
    return;
  }
  // Ohne Platz abgeschaltet: gelegentlich nachsehen, ob wieder Platz ist
  if (!enabled_ && !budgetDirty_ &&
      (now - lastBudgetCheck_).millis() >= static_cast<int32_t>(kBudgetRecheckMs))
    budgetDirty_ = true;
  if (!enabled_ && !budgetDirty_)
    return;
  if (budgetDirty_)
    enforceBudget();
  if (!bufLen_)
    return;
  const bool due = bufLen_ >= kFlushThreshold ||
                   (now - lastFlush_).millis() >= static_cast<int32_t>(kFlushIntervalMs);
  if (!due && !force)
    return;
  if (append())
    lastFlush_ = now;
}

bool ChronikService::writeTables(uint32_t epoch, bool (*fn)(void*, const uint8_t*, size_t),
                                 void* ctx) {
  uint8_t* frame = static_cast<uint8_t*>(hal::tempAlloc(kMaxFrame));
  if (!frame)
    return false;
  const SensorsConfig& sc = cfg_.sensors();
  uint8_t from = 0;
  bool ok = true;
  const uint8_t limit = board::kSlotCount < kMaxChannels ? board::kSlotCount : kMaxChannels;
  while (from < limit && ok) {
    TableBuilder b(frame, kMaxFrame, epoch);
    uint8_t s = from;
    for (; s < limit; s++) {
      const ChannelConfig& cc = sc.channels[s];
      if (!cc.enabled)
        continue;
      const auto& sl = board::kSlots[s];
      const ChannelDef d = catalog::channel(board::kSensors[sl.sensor].type, sl.channel);
      char key[kMaxText + 1];
      schema::keyOfSlot(s, key, sizeof(key));
      if (!b.add(s, d.hasRaw, d.oneSided, key, cc.name.c_str(), cc.unit.c_str(), cc.limits,
                 enc_.enc[s]))
        break;
    }
    if (s == from)
      break; // ein einzelner Kanal passt nicht: aufgeben
    const size_t len = b.finish();
    if (len)
      ok = fn(ctx, frame, len);
    from = s;
  }
  hal::tempFree(frame);
  return ok;
}

bool ChronikService::startSegment() {
  uint32_t index = range_.nextIndex();
  char name[kNameBuffer];
  nameFromIndex(index, name);
  for (uint8_t tries = 0; fs_.exists(name) && tries < 8; tries++) {
    LOGW(Mod::Chronik, "Segment existiert bereits: %s", name);
    nameFromIndex(++index, name);
  }
  hal::File* f = fs_.open(name, hal::OpenMode::Write);
  if (!f) {
    LOGE(Mod::Chronik, "Segment nicht anlegbar: %s", name);
    return false;
  }
  struct Ctx {
    hal::File* f;
    uint32_t written;
  } ctx{f, 0};
  const uint32_t ep = hal::epoch();
  const bool ok = writeTables(
      ep,
      [](void* c, const uint8_t* d, size_t n) {
        Ctx& x = *static_cast<Ctx*>(c);
        if (x.f->writeBytes(d, n) != n)
          return false;
        x.written += n;
        return true;
      },
      &ctx);
  fs_.close(f);
  if (!ok) {
    LOGE(Mod::Chronik, "Kanaltabelle unvollständig geschrieben");
    fs_.remove(name);
    return false;
  }
  range_.add(index);
  currentSize_ = ctx.written;
  enforceBudget();
  return true;
}

bool ChronikService::append() {
  if (range_.empty() || currentSize_ + bufLen_ > kSegmentSize) {
    if (!startSegment())
      return false;
    if (!enabled_) {
      bufLen_ = 0;
      return false;
    }
  }
  char name[kNameBuffer];
  nameFromIndex(range_.newest(), name);
  hal::FileGuard f(fs_, fs_.open(name, hal::OpenMode::Append));
  if (!f) {
    LOGE(Mod::Chronik, "Segment nicht zum Anhängen offen: %s", name);
    return false;
  }
  const size_t w = f->writeBytes(buf_, bufLen_);
  if (w != bufLen_) {
    LOGE(Mod::Chronik, "Nur %u von %u Bytes geschrieben - schaffe Platz", static_cast<unsigned>(w),
         static_cast<unsigned>(bufLen_));
    bufLen_ = 0;
    budgetDirty_ = true;
    f.reset();
    scan();
    return false;
  }
  currentSize_ += static_cast<uint32_t>(w);
  bufLen_ = 0;
  return true;
}

uint32_t ChronikService::firstEpochOf(uint32_t index) {
  char name[kNameBuffer];
  nameFromIndex(index, name);
  hal::FileGuard f(fs_, fs_.open(name, hal::OpenMode::Read));
  if (!f)
    return 0;
  uint8_t head[8];
  if (f->read(head, sizeof(head)) < 8)
    return 0;
  return epochOf(head);
}

bool ChronikService::stream(uint32_t since, Sink& out) {
  if (!range_.empty()) {
    const uint32_t oldest = range_.oldest(), newest = range_.newest();
    uint8_t chunk[kChunk];
    for (uint32_t idx = oldest; idx <= newest; idx++) {
      if (since > 0 && idx < newest) {
        const uint32_t next = firstEpochOf(idx + 1);
        if (next != 0 && next <= since)
          continue;
      }
      char name[kNameBuffer];
      nameFromIndex(idx, name);
      hal::FileGuard f(fs_, fs_.open(name, hal::OpenMode::Read));
      if (!f)
        continue;
      for (;;) {
        const size_t n = f->read(chunk, sizeof(chunk));
        if (!n)
          break;
        out.write(reinterpret_cast<const char*>(chunk), n);
        hal::yieldNow();
      }
    }
  }
  // Aktuelle Tabelle direkt vor dem RAM-Puffer: beschreibt dessen Rahmen
  writeTables(
      hal::epoch(),
      [](void* c, const uint8_t* d, size_t n) {
        static_cast<Sink*>(c)->write(reinterpret_cast<const char*>(d), n);
        return true;
      },
      &out);
  if (bufLen_)
    out.write(reinterpret_cast<const char*>(buf_), bufLen_);
  return true;
}

bool ChronikService::exportCsv(Sink& out) {
  void* mem = hal::tempAlloc(sizeof(CsvConverter));
  if (!mem)
    return false;
  CsvConverter* conv = new (mem) CsvConverter(out);
  conv->header();
  struct Feed : Sink {
    CsvConverter* c;
    void write(const char* d, size_t n) override {
      c->feed(reinterpret_cast<const uint8_t*>(d), n);
    }
    using Sink::write;
  } feed;
  feed.c = conv;
  stream(0, feed);
  conv->finish();
  conv->~CsvConverter();
  hal::tempFree(mem);
  return true;
}

void ChronikService::writeInfo(JsonWriter& w) const {
  w.beginObject();
  w.kv(FS_("segments"), static_cast<uint32_t>(range_.count()));
  w.kv(FS_("targetSegments"), static_cast<uint32_t>(target_));
  w.kv(FS_("segmentSize"), kSegmentSize);
  w.kv(FS_("usedBytes"), usedBytes());
  w.kv(FS_("enabled"), enabled());
  w.kv(FS_("reason"), bootDisabled_ ? "bootloop" : (enabled_ ? "" : "space"));
  w.kv(FS_("dropped"), dropped_);
  w.kv(FS_("skippedChannels"), static_cast<uint32_t>(skippedChannels()));
  w.endObject();
}

} // namespace ps
