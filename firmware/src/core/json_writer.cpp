/**
 * @file json_writer.cpp
 */
#include "core/json_writer.h"

namespace ps {

void Sink::writeP(FStr s) {
  if (!s.p)
    return;
  char buf[32];
  size_t n = 0;
  for (size_t i = 0;; i++) {
    const char c = static_cast<char>(ps_read_byte(s.p + i));
    if (!c)
      break;
    buf[n++] = c;
    if (n == sizeof(buf)) {
      write(buf, n);
      n = 0;
    }
  }
  if (n)
    write(buf, n);
}

namespace {
const char kHex[] = "0123456789abcdef";

void escapeChar(Sink& out, char c) {
  switch (c) {
  case '"':
    out.write("\\\"", 2);
    return;
  case '\\':
    out.write("\\\\", 2);
    return;
  case '\n':
    out.write("\\n", 2);
    return;
  case '\r':
    out.write("\\r", 2);
    return;
  case '\t':
    out.write("\\t", 2);
    return;
  default:
    break;
  }
  if (static_cast<uint8_t>(c) < 0x20) {
    char buf[6] = {'\\', 'u', '0', '0', kHex[(c >> 4) & 0xF], kHex[c & 0xF]};
    out.write(buf, 6);
    return;
  }
  out.write(&c, 1);
}
} // namespace

void jsonEscape(Sink& out, const char* s, size_t len) {
  if (!s)
    return;
  size_t start = 0;
  for (size_t i = 0; i < len; i++) {
    const char c = s[i];
    if (c == '"' || c == '\\' || static_cast<uint8_t>(c) < 0x20) {
      if (i > start)
        out.write(s + start, i - start);
      escapeChar(out, c);
      start = i + 1;
    }
  }
  if (len > start)
    out.write(s + start, len - start);
}

void jsonEscapeP(Sink& out, FStr s) {
  if (!s.p)
    return;
  for (size_t i = 0;; i++) {
    const char c = static_cast<char>(ps_read_byte(s.p + i));
    if (!c)
      break;
    escapeChar(out, c);
  }
}

size_t formatFloat(char* buf, size_t cap, float v, uint8_t decimals) {
  if (cap == 0)
    return 0;
  if (isnan(v) || isinf(v)) {
    const size_t n = cap > 4 ? 4 : cap - 1;
    memcpy(buf, "null", n);
    buf[n] = '\0';
    return n;
  }
  if (decimals > 6)
    decimals = 6;
  // Ganzzahlige Ausgabe ohne printf-Float (spart auf dem ESP8266 Stack und
  // ist exakt reproduzierbar).
  double d = v;
  const bool neg = d < 0;
  if (neg)
    d = -d;
  uint32_t scale = 1;
  for (uint8_t i = 0; i < decimals; i++)
    scale *= 10;
  double scaled = d * scale + 0.5;
  if (scaled >= 1.8e19) { // zu groß für uint64: in Exponentialschreibweise
    int n = snprintf(buf, cap, "%g", static_cast<double>(v));
    return n < 0 ? 0 : static_cast<size_t>(n);
  }
  uint64_t all = static_cast<uint64_t>(scaled);
  uint64_t ip = all / scale;
  uint32_t fp = static_cast<uint32_t>(all % scale);
  char tmp[32];
  size_t n = 0;
  // Nachkommastellen ohne abschließende Nullen
  uint8_t dec = decimals;
  while (dec > 0 && fp % 10 == 0) {
    fp /= 10;
    dec--;
  }
  for (uint8_t i = 0; i < dec; i++) {
    tmp[n++] = static_cast<char>('0' + fp % 10);
    fp /= 10;
  }
  if (dec > 0)
    tmp[n++] = '.';
  do {
    tmp[n++] = static_cast<char>('0' + ip % 10);
    ip /= 10;
  } while (ip > 0 && n < sizeof(tmp) - 1);
  if (neg && (all != 0))
    tmp[n++] = '-';
  size_t out = 0;
  while (n > 0 && out + 1 < cap)
    buf[out++] = tmp[--n];
  buf[out] = '\0';
  return out;
}

void JsonWriter::separator() {
  if (afterKey_) {
    afterKey_ = false;
    return;
  }
  if (depth_ == 0)
    return;
  const uint8_t bit = static_cast<uint8_t>(1u << ((depth_ - 1) & 7));
  if (hasItem_ & bit)
    out_.put(',');
  hasItem_ |= bit;
}

JsonWriter& JsonWriter::open(char c) {
  separator();
  out_.put(c);
  if (depth_ < 8) {
    depth_++;
    hasItem_ &= static_cast<uint8_t>(~(1u << ((depth_ - 1) & 7)));
  }
  return *this;
}

JsonWriter& JsonWriter::close(char c) {
  out_.put(c);
  if (depth_ > 0)
    depth_--;
  afterKey_ = false;
  return *this;
}

JsonWriter& JsonWriter::key(const char* k) {
  separator();
  out_.put('"');
  jsonEscape(out_, k, k ? strlen(k) : 0);
  out_.write("\":", 2);
  afterKey_ = true;
  return *this;
}

JsonWriter& JsonWriter::key(FStr k) {
  separator();
  out_.put('"');
  jsonEscapeP(out_, k);
  out_.write("\":", 2);
  afterKey_ = true;
  return *this;
}

JsonWriter& JsonWriter::keyParts(const char* a, char sep, const char* b) {
  separator();
  out_.put('"');
  jsonEscape(out_, a, strlen(a));
  out_.put(sep);
  jsonEscape(out_, b, strlen(b));
  out_.write("\":", 2);
  afterKey_ = true;
  return *this;
}

JsonWriter& JsonWriter::value(const char* s) { return value(s, s ? strlen(s) : 0); }

JsonWriter& JsonWriter::value(const char* s, size_t len) {
  separator();
  out_.put('"');
  jsonEscape(out_, s, len);
  out_.put('"');
  return *this;
}

JsonWriter& JsonWriter::value(FStr s) {
  separator();
  out_.put('"');
  jsonEscapeP(out_, s);
  out_.put('"');
  return *this;
}

JsonWriter& JsonWriter::value(bool b) {
  separator();
  if (b)
    out_.write("true", 4);
  else
    out_.write("false", 5);
  return *this;
}

JsonWriter& JsonWriter::valueUnsigned(uint64_t v) {
  separator();
  char tmp[21];
  size_t n = 0;
  do {
    tmp[n++] = static_cast<char>('0' + v % 10);
    v /= 10;
  } while (v);
  char buf[21];
  for (size_t i = 0; i < n; i++)
    buf[i] = tmp[n - 1 - i];
  out_.write(buf, n);
  return *this;
}

JsonWriter& JsonWriter::valueSigned(int64_t v) {
  if (v < 0) {
    separator();
    out_.put('-');
    afterKey_ = true; // kein zweites Komma
    return valueUnsigned(static_cast<uint64_t>(-(v + 1)) + 1);
  }
  return valueUnsigned(static_cast<uint64_t>(v));
}

JsonWriter& JsonWriter::value(float v, uint8_t decimals) {
  separator();
  char buf[32];
  const size_t n = formatFloat(buf, sizeof(buf), v, decimals);
  out_.write(buf, n);
  return *this;
}

JsonWriter& JsonWriter::null() {
  separator();
  out_.write("null", 4);
  return *this;
}

JsonWriter& JsonWriter::raw(const char* json) {
  separator();
  out_.write(json, strlen(json));
  return *this;
}

} // namespace ps
