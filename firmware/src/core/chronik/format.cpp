/**
 * @file format.cpp
 */
#include "core/chronik/format.h"

namespace ps {
namespace chronik {

namespace {
inline void putU8(uint8_t* d, size_t& at, uint8_t v) { d[at++] = v; }
inline void putU16(uint8_t* d, size_t& at, uint16_t v) {
  d[at++] = uint8_t(v);
  d[at++] = uint8_t(v >> 8);
}
inline void putU32(uint8_t* d, size_t& at, uint32_t v) {
  for (int i = 0; i < 4; i++)
    d[at++] = uint8_t(v >> (8 * i));
}
inline void putF32(uint8_t* d, size_t& at, float f) {
  uint32_t v;
  memcpy(&v, &f, 4);
  putU32(d, at, v);
}
inline uint16_t getU16(const uint8_t* s) { return uint16_t(s[0] | (s[1] << 8)); }
inline uint32_t getU32(const uint8_t* s) {
  return uint32_t(s[0]) | (uint32_t(s[1]) << 8) | (uint32_t(s[2]) << 16) | (uint32_t(s[3]) << 24);
}
inline float getF32(const uint8_t* s) {
  const uint32_t v = getU32(s);
  float f;
  memcpy(&f, &v, 4);
  return f;
}
} // namespace

uint16_t halfFromFloat(float value) {
  if (isnan(value))
    return 0x7E00;
  uint32_t bits;
  memcpy(&bits, &value, 4);
  const uint16_t sign = uint16_t((bits >> 16) & 0x8000);
  int32_t exponent = int32_t((bits >> 23) & 0xFF) - 127;
  uint32_t mantissa = bits & 0x7FFFFF;
  if (exponent == 128 || exponent > 15)
    return uint16_t(sign | 0x7C00);
  if (exponent < -24)
    return sign;
  if (exponent < -14) {
    mantissa |= 0x800000;
    const uint32_t shift = uint32_t(-14 - exponent) + 13;
    const uint32_t sub = mantissa >> shift;
    const uint32_t rest = mantissa & ((1UL << shift) - 1);
    const uint32_t half = 1UL << (shift - 1);
    return uint16_t(sign | (sub + ((rest > half || (rest == half && (sub & 1))) ? 1 : 0)));
  }
  uint16_t r = uint16_t(sign | uint16_t((exponent + 15) << 10) | (mantissa >> 13));
  const uint32_t rest = mantissa & 0x1FFF;
  if (rest > 0x1000 || (rest == 0x1000 && (r & 1)))
    r++;
  return r;
}

float floatFromHalf(uint16_t half) {
  const uint32_t sign = uint32_t(half & 0x8000) << 16;
  const uint32_t exponent = (half >> 10) & 0x1F;
  const uint32_t mantissa = half & 0x3FF;
  uint32_t bits;
  if (exponent == 0) {
    if (mantissa == 0) {
      bits = sign;
    } else {
      uint32_t e = 127 - 15 + 1;
      uint32_t m = mantissa;
      while ((m & 0x400) == 0) {
        m <<= 1;
        e--;
      }
      m &= 0x3FF;
      bits = sign | (e << 23) | (m << 13);
    }
  } else if (exponent == 0x1F) {
    bits = sign | 0x7F800000UL | (mantissa << 13);
  } else {
    bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
  }
  float v;
  memcpy(&v, &bits, 4);
  return v;
}

void encodeValue(const Encoding& e, float v, uint8_t* out) {
  size_t at = 0;
  switch (e.kind) {
  case ChronikEncoding::Float32:
    putF32(out, at, v);
    return;
  case ChronikEncoding::Int16Scaled: {
    int16_t s;
    if (isnan(v) || e.scale == 0) {
      s = INT16_MIN; // "kein Wert"
    } else {
      const float x = roundf((v - e.offset) / e.scale);
      s = x > 32767.0f ? int16_t(32767) : (x < -32767.0f ? int16_t(-32767) : int16_t(x));
    }
    putU16(out, at, uint16_t(s));
    return;
  }
  default:
    putU16(out, at, halfFromFloat(v));
  }
}

float decodeValue(const Encoding& e, const uint8_t* in) {
  switch (e.kind) {
  case ChronikEncoding::Float32:
    return getF32(in);
  case ChronikEncoding::Int16Scaled: {
    const int16_t s = int16_t(getU16(in));
    if (s == INT16_MIN)
      return NAN;
    return float(s) * e.scale + e.offset;
  }
  default:
    return floatFromHalf(getU16(in));
  }
}

size_t writeSample(const SampleFrame& f, const EncodingMap& map, uint8_t* dst, size_t space) {
  if (!dst || f.count > kMaxChannels)
    return 0;
  size_t payload = 0;
  for (uint8_t i = 0; i < f.count; i++) {
    const ChannelValue& v = f.values[i];
    if (v.channel >= kMaxChannels || !map.known[v.channel])
      return 0;
    payload += 1 + map.enc[v.channel].valueSize() + (v.hasRaw ? 2 : 0);
  }
  const size_t need = 2 + 4 + 1 + 1 + payload + 1;
  if (need > space || payload > 255)
    return 0;
  size_t at = 0;
  putU16(dst, at, kMagicSample);
  putU32(dst, at, f.epoch);
  putU8(dst, at, f.count);
  putU8(dst, at, uint8_t(payload));
  for (uint8_t i = 0; i < f.count; i++) {
    const ChannelValue& v = f.values[i];
    putU8(dst, at, uint8_t((v.channel & 0x0F) | ((v.status & 0x07) << 4) | (v.hasRaw ? 0x80 : 0)));
    encodeValue(map.enc[v.channel], v.value, dst + at);
    at += map.enc[v.channel].valueSize();
    if (v.hasRaw)
      putU16(dst, at, uint16_t(v.raw));
  }
  dst[at] = crc::crc8(dst, at);
  return at + 1;
}

size_t sampleLength(const uint8_t* src, size_t len) {
  if (!src || len < 9 || getU16(src) != kMagicSample)
    return 0;
  if (src[6] > kMaxChannels)
    return 0;
  const size_t total = 8 + size_t(src[7]) + 1;
  if (total > len)
    return 0;
  if (src[total - 1] != crc::crc8(src, total - 1))
    return 0;
  return total;
}

size_t readSample(const uint8_t* src, size_t len, const EncodingMap& map, SampleFrame& out) {
  const size_t total = sampleLength(src, len);
  if (!total)
    return 0;
  SampleFrame f;
  f.epoch = getU32(src + 2);
  f.count = src[6];
  const size_t end = total - 1;
  size_t at = 8;
  for (uint8_t i = 0; i < f.count; i++) {
    if (at + 1 > end)
      return 0;
    ChannelValue& v = f.values[i];
    const uint8_t head = src[at++];
    v.channel = head & 0x0F;
    v.status = (head >> 4) & 0x07;
    v.hasRaw = (head & 0x80) != 0;
    if (!map.known[v.channel])
      return 0;
    const Encoding& e = map.enc[v.channel];
    if (at + e.valueSize() > end)
      return 0;
    v.value = decodeValue(e, src + at);
    at += e.valueSize();
    if (v.hasRaw) {
      if (at + 2 > end)
        return 0;
      v.raw = int16_t(getU16(src + at));
      at += 2;
    } else {
      v.raw = -1;
    }
  }
  if (at != end)
    return 0;
  out = f;
  return total;
}

size_t readTable(const uint8_t* src, size_t len, TableCallback cb, void* ctx, EncodingMap* map) {
  if (!src || len < 8 || getU16(src) != kMagicTable)
    return 0;
  const uint8_t count = src[6];
  if (count > kMaxChannels)
    return 0;
  size_t at = 7;
  for (uint8_t i = 0; i < count; i++) {
    if (at + 2 > len)
      return 0;
    at += 2;
    for (uint8_t t = 0; t < 3; t++) {
      if (at >= len)
        return 0;
      const uint8_t l = src[at++];
      if (l > kMaxText || at + l > len)
        return 0;
      at += l;
    }
    if (at + 16 + 9 > len)
      return 0;
    at += 16 + 9;
  }
  if (at >= len || src[at] != crc::crc8(src, at))
    return 0;
  const size_t total = at + 1;

  at = 7;
  for (uint8_t i = 0; i < count; i++) {
    TableEntry e;
    e.channel = src[at++];
    const uint8_t flags = src[at++];
    e.analog = flags & 1;
    e.oneSided = (flags & 2) != 0;
    const char** texts[3] = {&e.key, &e.name, &e.unit};
    uint8_t* lens[3] = {&e.keyLen, &e.nameLen, &e.unitLen};
    for (uint8_t t = 0; t < 3; t++) {
      const uint8_t l = src[at++];
      *texts[t] = reinterpret_cast<const char*>(src + at);
      *lens[t] = l;
      at += l;
    }
    e.limits.yellowLow = getF32(src + at);
    e.limits.greenLow = getF32(src + at + 4);
    e.limits.greenHigh = getF32(src + at + 8);
    e.limits.yellowHigh = getF32(src + at + 12);
    at += 16;
    e.encoding.kind = static_cast<ChronikEncoding>(src[at] <= 2 ? src[at] : 0);
    e.encoding.scale = getF32(src + at + 1);
    e.encoding.offset = getF32(src + at + 5);
    at += 9;
    if (map && e.channel < kMaxChannels) {
      map->enc[e.channel] = e.encoding;
      map->known[e.channel] = true;
    }
    if (cb)
      cb(ctx, e);
  }
  return total;
}

TableBuilder::TableBuilder(uint8_t* dst, size_t space, uint32_t epoch)
    : dst_(dst), space_(space > kMaxFrame ? kMaxFrame : space) {
  if (!dst_ || space_ < 9) {
    ok_ = false;
    return;
  }
  putU16(dst_, at_, kMagicTable);
  putU32(dst_, at_, epoch);
  countAt_ = at_;
  putU8(dst_, at_, 0);
}

size_t TableBuilder::textLen(const char* s) {
  if (!s)
    return 0;
  size_t len = strnlen(s, kMaxText + 1);
  if (len <= kMaxText)
    return len;
  len = kMaxText;
  while (len > 0 && (uint8_t(s[len]) & 0xC0) == 0x80)
    len--;
  return len;
}

void TableBuilder::putText(const char* s) {
  const size_t l = textLen(s);
  dst_[at_++] = uint8_t(l);
  if (l)
    memcpy(dst_ + at_, s, l);
  at_ += l;
}

bool TableBuilder::add(uint8_t channel, bool analog, bool oneSided, const char* key,
                       const char* name, const char* unit, const Limits& l, const Encoding& e) {
  if (!ok_ || count_ >= kMaxChannels || channel >= kMaxChannels)
    return false;
  const size_t need = 2 + 3 + textLen(key) + textLen(name) + textLen(unit) + 16 + 9;
  if (at_ + need + 1 > space_)
    return false;
  putU8(dst_, at_, channel);
  putU8(dst_, at_, uint8_t((analog ? 1 : 0) | (oneSided ? 2 : 0)));
  putText(key);
  putText(name);
  putText(unit);
  putF32(dst_, at_, l.yellowLow);
  putF32(dst_, at_, l.greenLow);
  putF32(dst_, at_, l.greenHigh);
  putF32(dst_, at_, l.yellowHigh);
  putU8(dst_, at_, uint8_t(e.kind));
  putF32(dst_, at_, e.scale);
  putF32(dst_, at_, e.offset);
  count_++;
  return true;
}

size_t TableBuilder::finish() {
  if (!ok_ || count_ == 0 || at_ + 1 > space_)
    return 0;
  dst_[countAt_] = count_;
  dst_[at_] = crc::crc8(dst_, at_);
  return at_ + 1;
}

size_t frameLength(const uint8_t* src, size_t len) {
  if (!src || len < 2)
    return 0;
  const uint16_t m = getU16(src);
  if (m == kMagicSample)
    return sampleLength(src, len);
  if (m == kMagicTable)
    return readTable(src, len, nullptr, nullptr, nullptr);
  return 0;
}

size_t findMagic(const uint8_t* src, size_t len, size_t from) {
  if (!src || len < 2)
    return len;
  for (size_t i = from; i + 1 < len; i++) {
    const uint16_t m = getU16(src + i);
    if (m == kMagicSample || m == kMagicTable)
      return i;
  }
  return len;
}

size_t validPrefixLength(const uint8_t* src, size_t len) {
  size_t at = 0;
  while (at < len) {
    const size_t n = frameLength(src + at, len - at);
    if (!n)
      break;
    at += n;
  }
  return at;
}

} // namespace chronik
} // namespace ps
