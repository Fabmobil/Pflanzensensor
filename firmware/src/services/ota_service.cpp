/**
 * @file ota_service.cpp
 */
#include "services/ota_service.h"

#include "hal/system.h"
#include "services/log.h"

namespace ps {

Err OtaService::start(uint32_t size, const char* md5, Instant now) {
  if (active_)
    abort("neuer Upload");
  success_ = failed_ = false;
  error_.clear();
  if (cfg_.device().otaRequireMd5 && (!md5 || strlen(md5) != 32)) {
    fail("Prüfsumme (MD5) wird verlangt");
    return Err::InvalidArgument;
  }
  if (md5 && *md5 && strlen(md5) != 32) {
    fail("MD5 muss 32 Hex-Zeichen haben");
    return Err::InvalidArgument;
  }
  const uint32_t room = hal::freeSketchSpace();
  if (size > 0 && size > room) {
    fail("Datei zu groß für den freien Bereich");
    return Err::NoSpace;
  }
  if (prepare_)
    prepare_(ctx_, true);
  if (!ota_.begin(size, md5 && *md5 ? md5 : nullptr)) {
    fail(ota_.error());
    if (prepare_)
      prepare_(ctx_, false);
    return Err::Io;
  }
  active_ = true;
  firstChunk_ = true;
  size_ = size;
  written_ = 0;
  lastPercent_ = 255;
  minHeap_ = hal::heap().free;
  lastData_ = now;
  Event e;
  e.type = EventType::OtaStarted;
  events_.push(e);
  LOGI(Mod::Ota, "Update gestartet (%lu Bytes)", static_cast<unsigned long>(size));
  return Err::None;
}

void OtaService::data(const uint8_t* d, size_t len, Instant now) {
  if (!active_ || failed_)
    return;
  lastData_ = now;
  if (firstChunk_ && len > 0) {
    firstChunk_ = false;
    if (d[0] != 0xE9) { // Magic-Byte eines ESP-Images
      abort("Keine gültige Firmware-Datei (falsches Magic-Byte)");
      return;
    }
  }
  if (ota_.write(d, len) != len) {
    abort(ota_.error());
    return;
  }
  written_ += static_cast<uint32_t>(len);
  const uint32_t freeNow = hal::heap().free;
  if (freeNow < minHeap_)
    minHeap_ = freeNow;
  if (size_ && progress_) {
    const uint8_t p = static_cast<uint8_t>(static_cast<uint64_t>(written_) * 100 / size_);
    if (p != lastPercent_ && (p % 5 == 0 || p == 100)) {
      lastPercent_ = p;
      progress_(ctx_, p);
    }
  }
}

bool OtaService::finish(Instant now) {
  if (!active_)
    return false;
  active_ = false;
  if (failed_)
    return false;
  if (!ota_.end()) {
    fail(ota_.error());
    if (prepare_)
      prepare_(ctx_, false);
    return false;
  }
  success_ = true;
  LOGI(Mod::Ota, "Update erfolgreich (%lu Bytes, Heap min. %lu B) - Neustart folgt",
       static_cast<unsigned long>(written_), static_cast<unsigned long>(minHeap_));
  Event e;
  e.type = EventType::OtaFinished;
  e.a = 1;
  events_.push(e);
  (void)now;
  return true;
}

void OtaService::fail(const char* why) {
  failed_ = true;
  error_.assign(why ? why : "Fehler");
  LOGE(Mod::Ota, "Update fehlgeschlagen: %s (Heap min. %lu B)", error_.c_str(),
       static_cast<unsigned long>(minHeap_));
}

void OtaService::abort(const char* why) {
  if (active_)
    ota_.abort();
  const bool wasActive = active_;
  active_ = false;
  fail(why);
  if (wasActive && prepare_)
    prepare_(ctx_, false);
  Event e;
  e.type = EventType::OtaFinished;
  e.a = 0;
  events_.push(e);
}

void OtaService::tick(Instant now) {
  if (active_ && (now - lastData_).millis() >= static_cast<int32_t>(kIdleTimeoutMs))
    abort("Zeitüberschreitung: 60 s keine Daten");
}

void OtaService::writeStatus(JsonWriter& w) const {
  w.beginObject();
  w.kv(FS_("active"), active_);
  w.kv(FS_("success"), success_);
  w.kv(FS_("written"), written_);
  w.kv(FS_("size"), size_);
  w.kv(FS_("minHeap"), minHeap_);
  w.kv(FS_("error"), error_.c_str());
  w.endObject();
}

} // namespace ps
