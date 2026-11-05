#include "services/time_service.h"

#include "services/log.h"

namespace ps {

void TimeService::tick() {
  const bool now = epochValid(hal::epoch());
  if (now && !valid_) {
    valid_ = true;
    hal::LocalTime lt;
    if (hal::toLocalTime(hal::epoch(), lt))
      LOGI(Mod::Time, "Uhrzeit gestellt: %02u.%02u.%04u %02u:%02u", lt.day, lt.month, lt.year,
           lt.hour, lt.minute);
    Event e;
    e.type = EventType::ClockValid;
    events_.push(e);
  }
  valid_ = now;
}

} // namespace ps
