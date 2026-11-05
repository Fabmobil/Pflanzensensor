/**
 * @file throttle.h
 * @brief Mindestabstand zwischen zwei durchgelassenen Anfragen (RequestThrottle)
 * @details Abgewiesene Anfragen verlängern die Sperre nicht.
 */
#pragma once

#include <stdint.h>

namespace ps {

class RequestThrottle {
public:
  explicit constexpr RequestThrottle(uint32_t minIntervalMs) : min_(minIntervalMs) {}

  bool tryAcquire(uint32_t now) {
    if (fired_ && (now - last_) < min_)
      return false;
    last_ = now;
    fired_ = true;
    return true;
  }

  uint32_t remaining(uint32_t now) const {
    if (!fired_)
      return 0;
    const uint32_t el = now - last_;
    return el >= min_ ? 0 : min_ - el;
  }

private:
  uint32_t min_;
  uint32_t last_ = 0;
  bool fired_ = false;
};

/// Erlaubt eine Aktion höchstens einmal je Intervall (Log-Drossel).
class RateGate {
public:
  explicit constexpr RateGate(uint32_t intervalMs) : iv_(intervalMs) {}
  bool allow(uint32_t now) {
    if (!fired_ || now - last_ >= iv_) {
      fired_ = true;
      last_ = now;
      return true;
    }
    return false;
  }

private:
  uint32_t iv_;
  uint32_t last_ = 0;
  bool fired_ = false;
};

} // namespace ps
