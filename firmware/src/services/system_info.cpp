/**
 * @file system_info.cpp
 */
#include "services/system_info.h"

#include "services/log.h"

namespace ps {

void SystemInfo::begin() {
  hal::FileGuard f(fs_, fs_.open(kBootCountPath, hal::OpenMode::Read));
  if (f) {
    uint32_t v = 0;
    if (f->read(reinterpret_cast<uint8_t*>(&v), 4) == 4)
      reboots_ = v;
  }
  reboots_++;
  dirty_ = true;
  hal::fillRandom(reinterpret_cast<uint8_t*>(&bootId_), sizeof(bootId_));
  tick(hal::now());
}

void SystemInfo::persist() {
  if (!dirty_)
    return;
  dirty_ = false;
  hal::FileGuard f(fs_, fs_.open(kBootCountPath, hal::OpenMode::Write));
  if (f)
    f->writeBytes(reinterpret_cast<const uint8_t*>(&reboots_), 4);
}

void SystemInfo::tick(Instant now) {
  if (now.millis() < lastMs_)
    wraps_++;
  lastMs_ = now.millis();
  heap_ = hal::heap();
  if (heap_.free < minFree_)
    minFree_ = heap_.free;
  if (heap_.free < kCriticalHeap)
    LOGW(Mod::Mem, "Wenig Speicher: %lu B frei, größter Block %lu B",
         static_cast<unsigned long>(heap_.free), static_cast<unsigned long>(heap_.maxBlock));
  else
    LOGD(Mod::Mem, "Heap %lu B frei (min %lu), größter Block %lu, Fragmentierung %u%%",
         static_cast<unsigned long>(heap_.free), static_cast<unsigned long>(minFree_),
         static_cast<unsigned long>(heap_.maxBlock), heap_.fragmentation);
}

void SystemInfo::writeHeap(JsonWriter& w) const {
  w.kv(FS_("freeHeap"), heap_.free);
  w.kv(FS_("maxBlock"), heap_.maxBlock);
  w.kv(FS_("heapFragmentation"), static_cast<uint32_t>(heap_.fragmentation));
  w.kv(FS_("minFreeHeap"), minFree_);
}

} // namespace ps
