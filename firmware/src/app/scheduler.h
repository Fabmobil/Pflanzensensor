/**
 * @file scheduler.h
 * @brief Kooperativer Task-Takt mit Loop-Zeitmessung (refactor.md 5.5)
 */
#pragma once

#include "core/types.h"

namespace ps {

struct Task {
  void (*fn)(void* ctx, Instant now);
  void* ctx;
  int32_t periodMs; ///< 0: jede Schleife
  Instant next;
  const char* name;
};

class TaskScheduler {
public:
  static constexpr int32_t kWarnMs = 50;

  TaskScheduler(Task* tasks, uint8_t n) : tasks_(tasks), n_(n) {}
  void start(Instant now) {
    for (uint8_t i = 0; i < n_; i++)
      tasks_[i].next = now;
  }
  /// Fällige Tasks ausführen; gibt die längste Taskdauer zurück
  int32_t run(Instant (*clock)());
  uint32_t overruns() const { return overruns_; }
  int32_t maxLoopMs() const { return maxLoop_; }
  /// Längste Schleife seit dem letzten Aufruf (und der teuerste Task darin)
  int32_t takeWindowMax(const char** task) {
    const int32_t m = windowMax_;
    if (task)
      *task = windowTask_;
    windowMax_ = 0;
    windowTask_ = "";
    return m;
  }

private:
  Task* tasks_;
  uint8_t n_;
  uint32_t overruns_ = 0;
  int32_t maxLoop_ = 0;
  int32_t windowMax_ = 0;
  const char* windowTask_ = "";
};

} // namespace ps
