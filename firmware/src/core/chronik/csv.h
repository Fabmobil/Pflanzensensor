/**
 * @file csv.h
 * @brief Chronik-Datenstrom stückweise nach CSV umsetzen
 * @details Rahmen liegen quer über die Häppchengrenzen; ein Fenster sammelt
 *          Bytes, bis ein vollständiger Rahmen dekodiert werden kann. Nach
 *          einer Störstelle setzt der Leser am nächsten Magic wieder auf.
 *          Spalten: zeit,kanal,schluessel,wert,rohwert,status
 */
#pragma once

#include <stdio.h>

#include "core/chronik/format.h"
#include "core/json_writer.h"

namespace ps {
namespace chronik {

const char* statusText(uint8_t s);

class CsvConverter {
public:
  explicit CsvConverter(Sink& out) : out_(out) {}
  void header() { out_.write("zeit,kanal,schluessel,wert,rohwert,status\n"); }
  void feed(const uint8_t* data, size_t len);
  /// Rest verarbeiten (Ende des Stroms)
  void finish();

private:
  void process(bool final);
  static void onTable(void* ctx, const TableEntry& e);

  Sink& out_;
  uint8_t window_[kMaxFrame + 256];
  size_t len_ = 0;
  EncodingMap map_;
  char keys_[kMaxChannels][kMaxText + 1] = {};
};

} // namespace chronik
} // namespace ps
