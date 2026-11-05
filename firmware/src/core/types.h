/**
 * @file types.h
 * @brief Grundtypen des Kerns: Zeit, Strings fester Größe, Fehler, Ergebnisse
 * @details Arduino-frei. Alles hier hat feste Größe und allokiert nie.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string_view>

#include "platform/progmem.h"

namespace ps {

// ---------------------------------------------------------------------------
// Zeit
// ---------------------------------------------------------------------------

/// Zeitspanne in Millisekunden (vorzeichenbehaftet, damit Differenzen
/// sinnvoll sind).
class Duration {
public:
  constexpr Duration() = default;
  static constexpr Duration ms(int64_t v) { return Duration(static_cast<int32_t>(v)); }
  static constexpr Duration s(int64_t v) { return Duration(static_cast<int32_t>(v * 1000)); }
  static constexpr Duration min(int64_t v) { return Duration(static_cast<int32_t>(v * 60000)); }
  constexpr int32_t millis() const { return v_; }
  constexpr int32_t seconds() const { return v_ / 1000; }
  constexpr Duration operator+(Duration o) const { return Duration(v_ + o.v_); }
  constexpr Duration operator-(Duration o) const { return Duration(v_ - o.v_); }
  constexpr Duration operator*(int32_t k) const { return Duration(v_ * k); }
  constexpr bool operator<(Duration o) const { return v_ < o.v_; }
  constexpr bool operator<=(Duration o) const { return v_ <= o.v_; }
  constexpr bool operator>(Duration o) const { return v_ > o.v_; }
  constexpr bool operator>=(Duration o) const { return v_ >= o.v_; }
  constexpr bool operator==(Duration o) const { return v_ == o.v_; }
  constexpr bool operator!=(Duration o) const { return v_ != o.v_; }

private:
  constexpr explicit Duration(int32_t v) : v_(v) {}
  int32_t v_ = 0;
};

/**
 * @brief Zeitpunkt auf Basis von millis()
 * @details Vergleiche laufen über die vorzeichenbehaftete Differenz und sind
 *          damit über den Überlauf nach 49,7 Tagen hinweg korrekt, solange
 *          zwei verglichene Zeitpunkte weniger als 24,8 Tage auseinander
 *          liegen (Befund B-07).
 */
class Instant {
public:
  constexpr Instant() = default;
  static constexpr Instant fromMillis(uint32_t ms) { return Instant(ms); }
  constexpr uint32_t millis() const { return t_; }

  constexpr Instant operator+(Duration d) const {
    return Instant(t_ + static_cast<uint32_t>(d.millis()));
  }
  constexpr Instant operator-(Duration d) const {
    return Instant(t_ - static_cast<uint32_t>(d.millis()));
  }
  constexpr Duration operator-(Instant o) const {
    return Duration::ms(static_cast<int32_t>(t_ - o.t_));
  }
  /// true, wenn this mindestens so spät ist wie o
  constexpr bool reached(Instant o) const { return static_cast<int32_t>(t_ - o.t_) >= 0; }
  constexpr bool before(Instant o) const { return static_cast<int32_t>(t_ - o.t_) < 0; }
  constexpr bool operator==(Instant o) const { return t_ == o.t_; }
  constexpr bool operator!=(Instant o) const { return t_ != o.t_; }

private:
  constexpr explicit Instant(uint32_t t) : t_(t) {}
  uint32_t t_ = 0;
};

/// Unterhalb dieser Epoch gilt die Uhr als nicht gestellt (2020-09-13).
constexpr uint32_t kMinValidEpoch = 1600000000UL;
inline constexpr bool epochValid(uint32_t epoch) { return epoch >= kMinValidEpoch; }

// ---------------------------------------------------------------------------
// FixedString
// ---------------------------------------------------------------------------

/// Kürzt eine Länge so, dass kein UTF-8-Zeichen zerschnitten wird.
inline size_t utf8Truncate(const char* s, size_t len, size_t max) {
  if (len <= max)
    return len;
  size_t cut = max;
  while (cut > 0 && (static_cast<uint8_t>(s[cut]) & 0xC0) == 0x80)
    cut--;
  return cut;
}

/**
 * @brief Zeichenkette mit fester Kapazität (N inklusive Nullbyte)
 * @details Zu lange Eingaben werden an einer UTF-8-Zeichengrenze gekürzt;
 *          assign() meldet das über den Rückgabewert.
 */
template <size_t N> class FixedString {
  static_assert(N >= 2, "FixedString braucht Platz für mindestens ein Zeichen");

public:
  static constexpr size_t kCapacity = N - 1;

  constexpr FixedString() : buf_{} {}
  FixedString(const char* s) : buf_{} { assign(s); } // NOLINT: bewusst implizit

  /// @return false, wenn gekürzt werden musste
  bool assign(const char* s) { return assign(s, s ? strlen(s) : 0); }
  bool assign(std::string_view s) { return assign(s.data(), s.size()); }
  bool assign(const char* s, size_t len) {
    if (!s)
      len = 0;
    const size_t n = utf8Truncate(s, len, kCapacity);
    if (n)
      memmove(buf_, s, n);
    buf_[n] = '\0';
    return n == len;
  }
  FixedString& operator=(const char* s) {
    assign(s);
    return *this;
  }
  void clear() { buf_[0] = '\0'; }
  bool append(const char* s) {
    const size_t cur = size();
    const size_t len = s ? strlen(s) : 0;
    const size_t n = utf8Truncate(s, len, kCapacity - cur);
    memcpy(buf_ + cur, s, n);
    buf_[cur + n] = '\0';
    return n == len;
  }

  const char* c_str() const { return buf_; }
  char* data() { return buf_; }
  size_t size() const { return strnlen(buf_, N); }
  bool empty() const { return buf_[0] == '\0'; }
  std::string_view view() const { return std::string_view(buf_, size()); }
  bool operator==(const char* s) const { return s && strcmp(buf_, s) == 0; }
  bool operator!=(const char* s) const { return !(*this == s); }
  template <size_t M> bool operator==(const FixedString<M>& o) const {
    return strcmp(buf_, o.c_str()) == 0;
  }

private:
  char buf_[N];
};

// ---------------------------------------------------------------------------
// Fehler und Ergebnisse
// ---------------------------------------------------------------------------

/// Fehlercodes. Klartexte stehen in errText() (core/errors.cpp).
enum class Err : uint8_t {
  None = 0,
  Unknown,
  InvalidArgument,
  OutOfRange,
  TooLong,
  NotFound,
  Busy,
  NotReady,
  Disabled,
  NoSpace,
  Io,
  Corrupt,
  Timeout,
  Unsupported,
  Auth,
  Throttled,
  Network,
  NoClock,
  Config,
  Crypto,
  Hardware,
  ParseError,
};

/// Kurzer, maschinenlesbarer Code (für JSON-Fehlerantworten), im Flash.
FStr errCode(Err e);
/// Deutscher Klartext, im Flash.
FStr errText(Err e);

/// Ergebnis ohne Nutzwert.
class Status {
public:
  constexpr Status() = default;
  constexpr Status(Err e) : err_(e) {} // NOLINT: bewusst implizit
  static constexpr Status ok() { return Status(); }
  constexpr bool isOk() const { return err_ == Err::None; }
  constexpr explicit operator bool() const { return isOk(); }
  constexpr Err error() const { return err_; }

private:
  Err err_ = Err::None;
};

/// Ergebnis mit Nutzwert T (T muss default-konstruierbar sein).
template <typename T> class Result {
public:
  Result(const T& v) : value_(v), err_(Err::None) {} // NOLINT
  Result(Err e) : value_(), err_(e) {}               // NOLINT
  bool isOk() const { return err_ == Err::None; }
  explicit operator bool() const { return isOk(); }
  Err error() const { return err_; }
  const T& value() const { return value_; }
  T& value() { return value_; }
  T valueOr(const T& fallback) const { return isOk() ? value_ : fallback; }

private:
  T value_;
  Err err_;
};

/// Schrittergebnis nicht-blockierender Operationen.
enum class Step : uint8_t { Pending, Ok, Error };

} // namespace ps
