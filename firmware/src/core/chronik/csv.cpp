/**
 * @file csv.cpp
 */
#include "core/chronik/csv.h"

namespace ps {
namespace chronik {

const char* statusText(uint8_t s) {
  switch (s) {
  case kGreen:
    return "green";
  case kYellow:
    return "yellow";
  case kRed:
    return "red";
  case kError:
    return "error";
  case kWarmup:
    return "warmup";
  default:
    return "unknown";
  }
}

void CsvConverter::onTable(void* ctx, const TableEntry& e) {
  CsvConverter* self = static_cast<CsvConverter*>(ctx);
  if (e.channel >= kMaxChannels)
    return;
  memcpy(self->keys_[e.channel], e.key, e.keyLen);
  self->keys_[e.channel][e.keyLen] = '\0';
}

void CsvConverter::feed(const uint8_t* data, size_t len) {
  while (len > 0) {
    const size_t room = sizeof(window_) - len_;
    const size_t take = len < room ? len : room;
    memcpy(window_ + len_, data, take);
    len_ += take;
    data += take;
    len -= take;
    process(false);
    if (take == 0) { // Fenster voll und nichts verarbeitbar: beschädigt
      len_ = 0;
    }
  }
}

void CsvConverter::finish() { process(true); }

void CsvConverter::process(bool final) {
  size_t at = 0;
  while (at < len_) {
    const size_t rest = len_ - at;
    size_t n = readTable(window_ + at, rest, onTable, this, &map_);
    if (n) {
      at += n;
      continue;
    }
    SampleFrame f;
    n = readSample(window_ + at, rest, map_, f);
    if (n) {
      for (uint8_t i = 0; i < f.count; i++) {
        const ChannelValue& v = f.values[i];
        char val[24];
        formatFloat(val, sizeof(val), v.value, 3);
        if (isnan(v.value))
          val[0] = '\0';
        char line[96];
        char raw[8] = "";
        if (v.hasRaw)
          snprintf(raw, sizeof(raw), "%d", v.raw);
        const int len = snprintf(line, sizeof(line), "%lu,%u,%s,%s,%s,%s\n",
                                 static_cast<unsigned long>(f.epoch), v.channel, keys_[v.channel],
                                 val, raw, statusText(v.status));
        if (len > 0)
          out_.write(line, static_cast<size_t>(len) < sizeof(line) ? static_cast<size_t>(len)
                                                                   : sizeof(line) - 1);
      }
      at += n;
      continue;
    }
    // Kein vollständiger Rahmen: auf Nachschub warten oder neu aufsetzen
    const size_t frameLen = sampleLength(window_ + at, rest);
    if (!final && rest < kMaxFrame && frameLen == 0 && findMagic(window_ + at, rest, 0) == 0)
      break;
    const size_t next = findMagic(window_ + at, rest, 1);
    at += next >= rest ? rest : next;
  }
  if (at > 0) {
    memmove(window_, window_ + at, len_ - at);
    len_ -= at;
  }
}

} // namespace chronik
} // namespace ps
