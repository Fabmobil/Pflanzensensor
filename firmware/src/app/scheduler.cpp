/**
 * @file scheduler.cpp
 */
#include "app/scheduler.h"

#include "services/log.h"

namespace ps {

int32_t TaskScheduler::run(Instant (*clock)()) {
  const Instant loopStart = clock();
  int32_t longest = 0;
  const char* longestName = "";
  for (uint8_t i = 0; i < n_; i++) {
    Task& t = tasks_[i];
    const Instant now = clock();
    if (t.periodMs > 0 && !now.reached(t.next))
      continue;
    t.fn(t.ctx, now);
    const Instant end = clock();
    const int32_t took = (end - now).millis();
    if (took > longest) {
      longest = took;
      longestName = t.name;
    }
    if (took > kWarnMs) {
      overruns_++;
      LOGD(Mod::Main, "Task %s brauchte %ld ms", t.name, static_cast<long>(took));
    }
    if (t.periodMs > 0) {
      t.next = t.next + Duration::ms(t.periodMs);
      if (t.next.before(end))
        t.next = end + Duration::ms(t.periodMs); // kein Nachholen
    }
  }
  const int32_t loop = (clock() - loopStart).millis();
  if (loop > maxLoop_)
    maxLoop_ = loop;
  if (loop > windowMax_) {
    windowMax_ = loop;
    windowTask_ = longestName;
  }
  return longest;
}

} // namespace ps
