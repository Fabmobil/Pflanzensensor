/**
 * @file format.h
 * @brief Chronik-Rahmenformat v2 (refactor.md 5.8)
 * @details Übernommen aus dem Altcode (utils/chronik_format.h) und erweitert:
 *
 *  Kanaltabelle (Magic 0xC6A7), steht am Anfang jedes Segments:
 *    u16 magic | u32 epoch | u8 count |
 *    count × { u8 channel | u8 flags(bit0 analog, bit1 einseitig) |
 *              u8 len + key | u8 len + name | u8 len + unit |
 *              f32 yl | f32 gl | f32 gh | f32 yh |
 *              u8 encoding | f32 scale | f32 offset } | u8 crc8
 *
 *  Messrahmen (Magic 0xC6A5):
 *    u16 magic | u32 epoch | u8 count | u8 payloadLen |
 *    count × { u8 head(bit0-3 Kanal, bit4-6 Status, bit7 Rohwert) |
 *              Wert nach Kanal-Encoding (half 2 B, int16 2 B, float32 4 B) |
 *              [u16 Rohwert] } | u8 crc8
 *
 *  Die Länge im Messrahmen macht ihn ohne Tabelle abgrenzbar (Startscan,
 *  Wiederaufsetzen); zum Dekodieren der Werte braucht der Leser die
 *  vorausgehende Tabelle. Alles little-endian, von Hand gepackt.
 *  Neue Magic-Werte: Segmente des Altformats werden sicher verworfen.
 */
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/sensors/catalog.h"
#include "core/util/crc.h"

namespace ps {
namespace chronik {

constexpr uint16_t kMagicSample = 0xC6A5;
constexpr uint16_t kMagicTable = 0xC6A7;
constexpr uint8_t kMaxChannels = 16;
constexpr uint8_t kMaxText = 31;
constexpr size_t kMaxFrame = 512;
constexpr uint32_t kMinEpoch = 1600000000UL;

enum Status : uint8_t { kGreen = 0, kYellow, kRed, kError, kUnknown, kWarmup };

uint16_t halfFromFloat(float v);
float floatFromHalf(uint16_t h);

/// Kodierung eines Kanals (aus der Tabelle)
struct Encoding {
  ChronikEncoding kind = ChronikEncoding::Half;
  float scale = 1.0f;
  float offset = 0.0f;
  uint8_t valueSize() const { return kind == ChronikEncoding::Float32 ? 4 : 2; }
};

void encodeValue(const Encoding& e, float v, uint8_t* out);
float decodeValue(const Encoding& e, const uint8_t* in);

struct ChannelValue {
  uint8_t channel = 0;
  uint8_t status = kUnknown;
  float value = NAN;
  bool hasRaw = false;
  int16_t raw = -1;
};

struct SampleFrame {
  uint32_t epoch = 0;
  uint8_t count = 0;
  ChannelValue values[kMaxChannels];
};

/// Liefert die Kodierung je Kanalnummer (aus der zuletzt gelesenen Tabelle)
struct EncodingMap {
  Encoding enc[kMaxChannels];
  bool known[kMaxChannels] = {};
};

/**
 * @brief Messrahmen schreiben
 * @return Bytes oder 0, wenn der Platz nicht reicht (dst unverändert)
 */
size_t writeSample(const SampleFrame& f, const EncodingMap& map, uint8_t* dst, size_t space);
/// Länge eines gültigen Messrahmens oder 0
size_t sampleLength(const uint8_t* src, size_t len);
/**
 * @brief Messrahmen lesen
 * @return Bytes oder 0 (ungültig oder Kanal ohne bekannte Kodierung)
 */
size_t readSample(const uint8_t* src, size_t len, const EncodingMap& map, SampleFrame& out);

/// Ein Tabelleneintrag beim Lesen; Texte zeigen in den Puffer (nicht nullterminiert)
struct TableEntry {
  uint8_t channel = 0;
  bool analog = false;
  bool oneSided = false;
  const char* key = nullptr;
  uint8_t keyLen = 0;
  const char* name = nullptr;
  uint8_t nameLen = 0;
  const char* unit = nullptr;
  uint8_t unitLen = 0;
  Limits limits;
  Encoding encoding;
};

using TableCallback = void (*)(void* ctx, const TableEntry& e);

/// Tabelle lesen; ruft cb je Eintrag und füllt optional die Kodierungen
size_t readTable(const uint8_t* src, size_t len, TableCallback cb, void* ctx, EncodingMap* map);

/// Baut eine Kanaltabelle direkt im Zielpuffer
class TableBuilder {
public:
  TableBuilder(uint8_t* dst, size_t space, uint32_t epoch);
  bool add(uint8_t channel, bool analog, bool oneSided, const char* key, const char* name,
           const char* unit, const Limits& l, const Encoding& e);
  size_t finish();
  uint8_t count() const { return count_; }

private:
  static size_t textLen(const char* s);
  void putText(const char* s);
  uint8_t* dst_;
  size_t space_;
  size_t at_ = 0;
  size_t countAt_ = 0;
  uint8_t count_ = 0;
  bool ok_ = true;
};

/// Länge eines gültigen Rahmens beliebigen Typs oder 0
size_t frameLength(const uint8_t* src, size_t len);
/// Nächstes Magic ab from (oder len)
size_t findMagic(const uint8_t* src, size_t len, size_t from);
/// Länge des gültigen Anfangs
size_t validPrefixLength(const uint8_t* src, size_t len);
/// Epoche aus den ersten 6 Byte eines Rahmens (beide Typen)
inline uint32_t epochOf(const uint8_t* head) {
  return uint32_t(head[2]) | (uint32_t(head[3]) << 8) | (uint32_t(head[4]) << 16) |
         (uint32_t(head[5]) << 24);
}

} // namespace chronik
} // namespace ps
