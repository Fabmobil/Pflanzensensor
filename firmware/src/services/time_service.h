/**
 * @file time_service.h
 * @brief SNTP mit Zeitzone Europe/Berlin; nie blockierend (refactor.md 5.12)
 */
#pragma once

#include "core/util/event_queue.h"
#include "hal/system.h"

namespace ps {

class TimeService {
public:
  static constexpr const char* kTz = "CET-1CEST,M3.5.0/2,M10.5.0/3";
  static constexpr const char* kServer = "pool.ntp.org";

  explicit TimeService(EventQueue<16>& events) : events_(events) {}
  void begin() { hal::timeBegin(kTz, kServer); }
  /// Sekündlich
  void tick();
  bool valid() const { return valid_; }

private:
  EventQueue<16>& events_;
  bool valid_ = false;
};

} // namespace ps
