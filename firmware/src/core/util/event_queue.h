/**
 * @file event_queue.h
 * @brief Ereignisse zwischen den Modulen, verteilt in der Hauptschleife
 */
#pragma once

#include <stdint.h>

namespace ps {

enum class EventType : uint8_t {
  None = 0,
  MeasurementDone,     ///< a = Sensorindex
  SensorFailed,        ///< a = Sensorindex
  ConfigChanged,       ///< a = ConfigSection-Bitmaske
  SensorConfigChanged, ///< a = Sensorindex oder 0xFF
  ClockValid,
  WifiUp,
  WifiDown,
  ApStarted,
  RebootRequested, ///< a = Grund
  FactoryResetRequested,
  OtaStarted,
  OtaFinished, ///< a = 1 erfolgreich
};

struct Event {
  EventType type = EventType::None;
  uint8_t a = 0;
  uint8_t b = 0;
  uint32_t v = 0;
};

template <uint8_t N> class EventQueue {
public:
  bool push(const Event& e) {
    if (count_ >= N) {
      dropped_++;
      return false;
    }
    q_[(head_ + count_) % N] = e;
    count_++;
    return true;
  }
  bool pop(Event& e) {
    if (count_ == 0)
      return false;
    e = q_[head_];
    head_ = static_cast<uint8_t>((head_ + 1) % N);
    count_--;
    return true;
  }
  uint8_t size() const { return count_; }
  uint32_t dropped() const { return dropped_; }

private:
  Event q_[N];
  uint8_t head_ = 0;
  uint8_t count_ = 0;
  uint32_t dropped_ = 0;
};

} // namespace ps
