/**
 * @file chronik_format.h
 * @brief Rahmenformat der Chronik: Messwerte kompakt und selbstbeschreibend
 * @details Header-only und ohne jede Hardwareabhängigkeit - aus demselben
 *          Grund wie utils/crc32.h und web/core/request_throttle.h: so ist die
 *          Datei nativ testbar, ohne dass sie in den build_src_filter von
 *          [env:native] muss und dabei ihre Abhängigkeiten mitschleppt.
 *
 *          Warum ein Binärformat und kein CSV: das Dateisystem hat rund 240 KB
 *          frei, geteilt mit dem Datei-Log. Ein Messzyklus mit vier Kanälen
 *          kostet hier 24 Byte, als CSV wären es etwa 120 - also fünfmal so
 *          viel Historie aus demselben Platz. Umgerechnet wird im Browser;
 *          zehntausende Floats auf dem ESP zu Text zu machen wäre langsam und
 *          heap-lastig.
 *
 *          Alle Mehrbytewerte little-endian, alles von Hand Byte für Byte
 *          gepackt. Kein memcpy von structs: das Format wird von JavaScript
 *          gegengelesen und darf nicht von Padding oder Ausrichtung abhängen.
 */

#ifndef CHRONIK_FORMAT_H
#define CHRONIK_FORMAT_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ChronikFormat {

/// Messrahmen: ein Messzyklus mit allen Kanälen des Sensors
static constexpr uint16_t MAGIC_SAMPLE = 0xC5A5;
/// Kanaltabelle: steht am Anfang jedes Segments und beschreibt die Kanäle
static constexpr uint16_t MAGIC_TABLE = 0xC5A7;

/// Ein Kanalindex belegt vier Bit im Kopfbyte.
static constexpr uint8_t MAX_CHANNELS = 16;
/// Längste Zeichenkette in der Kanaltabelle (Schlüssel, Name, Einheit).
static constexpr uint8_t MAX_TEXT = 31;

/// Obergrenze für einen einzelnen Rahmen.
///
/// Der Startscan prüft das jüngste Segment mit einem gleitenden Fenster dieser
/// Größe - ein Segment ist 7936 B groß und passt nicht in den Heap eines
/// Geräts mit 16 KB frei. Solange kein Rahmen größer werden kann als das
/// Fenster, enthält jedes Fenster garantiert einen vollständigen Rahmen.
///
/// Praktische Auswirkung nur auf die Kanaltabelle: vier Kanäle brauchen rund
/// 160 B, acht rund 320. Bei sehr vielen Kanälen mit langen Namen passen nicht
/// alle in eine Tabelle - der TableBuilder bricht dann ab, statt einen zu
/// großen Rahmen zu erzeugen.
static constexpr size_t MAX_FRAME_SIZE = 512;

/// Zeitstempel unterhalb dieser Schwelle sind keine echte Uhrzeit, sondern der
/// Zustand "NTP hat nie synchronisiert" (2020-09-13). Rahmen mit solchen
/// Stempeln werden gar nicht erst geschrieben.
static constexpr uint32_t MIN_PLAUSIBLE_EPOCH = 1600000000UL;

/// Statuswerte, wie sie Sensor::getStatus() liefert - hier auf drei Bit gepackt.
enum Status : uint8_t {
  STATUS_GREEN = 0,
  STATUS_YELLOW = 1,
  STATUS_RED = 2,
  STATUS_ERROR = 3,
  STATUS_UNKNOWN = 4,
  STATUS_WARMUP = 5
};

/**
 * @brief CRC-8 (Polynom 0x07, Startwert 0x00) - der klassische "CRC-8/ATM"
 * @details Ein Byte Prüfsumme je Rahmen. Zusammen mit dem Magic reicht das, um
 *          beim Start einen halb geschriebenen Rahmen zu erkennen und um beim
 *          Lesen nach einer Störstelle wieder aufzusetzen.
 */
inline uint8_t crc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

/**
 * @brief float nach IEEE-754 half (16 Bit)
 * @details Zwei Byte statt vier je Messwert - bei vier Kanälen im Minutentakt
 *          sind das 11 KB pro Tag weniger. half deckt ±65504 mit rund 0,05 %
 *          relativer Auflösung ab: 25,37 °C wird zu 25,375, 100,0 % bleibt
 *          exakt, 5000 ppm bleibt exakt.
 *
 *          Bewusst NICHT int16 mit festem Faktor: ×10 liefe bei einem
 *          MHZ19 (bis 5000 ppm) über, und ein kanalabhängiger Faktor würde das
 *          Dateiformat an die zur Laufzeit änderbare Konfiguration koppeln -
 *          alte Daten wären nach einer Umstellung falsch skaliert.
 *
 *          Grenze fürs Protokoll: bei Luftdruck um 1013 hPa beträgt die
 *          Quantisierung 0,5 hPa. Für Trendkurven unerheblich, für
 *          Wetterableitungen zu grob.
 */
inline uint16_t halfFromFloat(float value) {
  if (isnan(value)) {
    return 0x7E00; // stiller NaN
  }

  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));

  const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000);
  int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFF) - 127;
  uint32_t mantissa = bits & 0x7FFFFF;

  if (exponent == 128) { // Inf
    return static_cast<uint16_t>(sign | 0x7C00);
  }
  if (exponent > 15) { // zu groß für half -> Inf, statt still zu verfälschen
    return static_cast<uint16_t>(sign | 0x7C00);
  }
  if (exponent < -24) { // zu klein selbst für subnormal
    return sign;
  }

  if (exponent < -14) { // subnormaler Bereich
    mantissa |= 0x800000;
    const uint32_t shift = static_cast<uint32_t>(-14 - exponent) + 13;
    const uint32_t sub = mantissa >> shift;
    // kaufmännisch runden, damit sich Fehler nicht systematisch aufsummieren
    const uint32_t rest = mantissa & ((1UL << shift) - 1);
    const uint32_t half = 1UL << (shift - 1);
    return static_cast<uint16_t>(sign |
                                 (sub + ((rest > half || (rest == half && (sub & 1))) ? 1 : 0)));
  }

  uint16_t result =
      static_cast<uint16_t>(sign | static_cast<uint16_t>((exponent + 15) << 10) | (mantissa >> 13));
  const uint32_t rest = mantissa & 0x1FFF;
  if (rest > 0x1000 || (rest == 0x1000 && (result & 1))) {
    result++; // trägt bei Bedarf sauber in den Exponenten hinein
  }
  return result;
}

/// @brief IEEE-754 half (16 Bit) zurück nach float
inline float floatFromHalf(uint16_t half) {
  const uint32_t sign = static_cast<uint32_t>(half & 0x8000) << 16;
  const uint32_t exponent = (half >> 10) & 0x1F;
  const uint32_t mantissa = half & 0x3FF;

  uint32_t bits;
  if (exponent == 0) {
    if (mantissa == 0) {
      bits = sign; // ±0
    } else {
      // Subnormal: normalisieren, bis das führende Bit herausgeschoben ist
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
    bits = sign | 0x7F800000UL | (mantissa << 13); // Inf oder NaN
  } else {
    bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
  }

  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

/// Ein Messwert innerhalb eines Messrahmens.
struct ChannelValue {
  uint8_t channel{0};             ///< Kanalindex, 0..15
  uint8_t status{STATUS_UNKNOWN}; ///< Status zum Messzeitpunkt
  float value{0.0f};              ///< Messwert
  bool hasRaw{false};             ///< Rohwert vorhanden (nur Analogsensoren)
  int16_t raw{-1};                ///< ADC-Rohwert 0..1023
};

/**
 * @brief Ein Messzyklus mit allen Kanälen des Sensors
 * @details Bewusst klein gehalten (rund 200 Byte): dieser Rahmen wird im
 *          Messzyklus auf dem Stack gebaut, und der ESP8266 hat davon nur 4 KB.
 *          Die Kanaltabelle ist deshalb KEIN Feld hier, sondern wird mit dem
 *          TableBuilder direkt in den Zielpuffer geschrieben - als Struktur
 *          wäre sie mit 16 Kanälen fast zwei Kilobyte groß.
 */
struct SampleFrame {
  uint32_t epoch{0};
  uint8_t count{0};
  ChannelValue values[MAX_CHANNELS];
};

namespace detail {

inline void putU8(uint8_t* dst, size_t& at, uint8_t v) { dst[at++] = v; }

inline void putU16(uint8_t* dst, size_t& at, uint16_t v) {
  dst[at++] = static_cast<uint8_t>(v & 0xFF);
  dst[at++] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

inline void putU32(uint8_t* dst, size_t& at, uint32_t v) {
  dst[at++] = static_cast<uint8_t>(v & 0xFF);
  dst[at++] = static_cast<uint8_t>((v >> 8) & 0xFF);
  dst[at++] = static_cast<uint8_t>((v >> 16) & 0xFF);
  dst[at++] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

inline uint16_t getU16(const uint8_t* src) { return static_cast<uint16_t>(src[0] | (src[1] << 8)); }

inline uint32_t getU32(const uint8_t* src) {
  return static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) |
         (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);
}

} // namespace detail

/// @brief Wieviele Bytes belegt dieser Messrahmen geschrieben?
inline size_t sampleFrameSize(const SampleFrame& frame) {
  size_t size = 2 + 4 + 1; // magic + epoch + count
  for (uint8_t i = 0; i < frame.count; i++) {
    size += 1 + 2 + (frame.values[i].hasRaw ? 2 : 0);
  }
  return size + 1; // crc8
}

/**
 * @brief Messrahmen in einen Puffer schreiben
 * @param space Verfügbarer Platz in dst
 * @return Anzahl geschriebener Bytes, 0 wenn der Platz nicht reicht. Bei 0
 *         wird dst nicht angefasst - ein Fragment in der Datei könnte kein
 *         Leser einordnen.
 */
inline size_t writeSample(const SampleFrame& frame, uint8_t* dst, size_t space) {
  if (!dst || frame.count > MAX_CHANNELS) {
    return 0;
  }
  const size_t needed = sampleFrameSize(frame);
  if (needed > space) {
    return 0;
  }

  size_t at = 0;
  detail::putU16(dst, at, MAGIC_SAMPLE);
  detail::putU32(dst, at, frame.epoch);
  detail::putU8(dst, at, frame.count);
  for (uint8_t i = 0; i < frame.count; i++) {
    const ChannelValue& value = frame.values[i];
    const uint8_t head = static_cast<uint8_t>(
        (value.channel & 0x0F) | ((value.status & 0x07) << 4) | (value.hasRaw ? 0x80 : 0x00));
    detail::putU8(dst, at, head);
    detail::putU16(dst, at, halfFromFloat(value.value));
    if (value.hasRaw) {
      detail::putU16(dst, at, static_cast<uint16_t>(value.raw));
    }
  }

  dst[at] = crc8(dst, at);
  at++;
  return at;
}

/**
 * @brief Messrahmen aus einem Puffer lesen
 * @return Anzahl gelesener Bytes, 0 wenn an dieser Stelle kein gültiger,
 *         vollständiger Messrahmen steht. Es wird nie über length hinaus
 *         gelesen. Eine Kanaltabelle liefert ebenfalls 0 - dafür gibt es
 *         readTable().
 */
inline size_t readSample(const uint8_t* src, size_t length, SampleFrame& out) {
  if (!src || length < 8) { // magic + epoch + count + crc
    return 0;
  }
  if (detail::getU16(src) != MAGIC_SAMPLE) {
    return 0;
  }
  const uint8_t count = src[6];
  if (count > MAX_CHANNELS) {
    return 0;
  }

  SampleFrame frame;
  frame.epoch = detail::getU32(src + 2);
  frame.count = count;

  size_t at = 7;
  for (uint8_t i = 0; i < count; i++) {
    if (at + 3 > length)
      return 0;
    ChannelValue& value = frame.values[i];
    const uint8_t head = src[at++];
    value.channel = static_cast<uint8_t>(head & 0x0F);
    value.status = static_cast<uint8_t>((head >> 4) & 0x07);
    value.hasRaw = (head & 0x80) != 0;
    value.value = floatFromHalf(detail::getU16(src + at));
    at += 2;
    if (value.hasRaw) {
      if (at + 2 > length)
        return 0;
      value.raw = static_cast<int16_t>(detail::getU16(src + at));
      at += 2;
    } else {
      value.raw = -1;
    }
  }

  if (at >= length || src[at] != crc8(src, at)) {
    return 0;
  }

  // Erst hier zuweisen: ein Aufrufer, der den Rückgabewert prüft, soll bei
  // einem beschädigten Rahmen seinen alten Inhalt unverändert behalten.
  out = frame;
  return at + 1;
}

/**
 * @class TableBuilder
 * @brief Schreibt die Kanaltabelle Kanal für Kanal direkt in den Zielpuffer
 * @details Am Anfang jedes Segments steht eine Tabelle, die alle vorkommenden
 *          Kanäle mit Schlüssel, Name, Einheit und den damals gültigen
 *          Schwellwerten beschreibt. Damit ist jedes Segment für sich lesbar:
 *          es braucht keine Zuordnungsdatei im Flash, Sensoränderungen
 *          verschieben nichts, und das Diagramm kann das Grünband zeichnen,
 *          das zum Messzeitpunkt galt.
 *
 *          Als Struktur wäre die Tabelle mit 16 Kanälen fast zwei Kilobyte
 *          groß - zuviel für den Stack. Deshalb dieser Bauer, der die Anzahl
 *          am Ende an ihre bereits reservierte Stelle nachträgt.
 */
class TableBuilder {
public:
  TableBuilder(uint8_t* dst, size_t space, uint32_t epoch)
      : m_dst(dst), m_space(space > MAX_FRAME_SIZE ? MAX_FRAME_SIZE : space) {
    if (!m_dst || m_space < 8) {
      m_ok = false;
      return;
    }
    detail::putU16(m_dst, m_at, MAGIC_TABLE);
    detail::putU32(m_dst, m_at, epoch);
    m_countAt = m_at;
    detail::putU8(m_dst, m_at, 0); // Anzahl wird in finish() nachgetragen
  }

  /**
   * @brief Einen Kanal anfügen
   * @return false, wenn er nicht mehr hineinpasst - der Rahmen bleibt dann
   *         unverändert und der Aufrufer beginnt einen neuen. Genau so
   *         verteilt ChronikRecorder eine große Kanaltabelle auf mehrere
   *         Rahmen, statt sie ab einer bestimmten Sensorzahl still fallen zu
   *         lassen.
   */
  bool addChannel(uint8_t channel, bool analog, bool oneSided, const char* key, const char* name,
                  const char* unit, float yellowLow, float greenLow, float greenHigh,
                  float yellowHigh) {
    if (!m_ok || m_count >= MAX_CHANNELS) {
      return false;
    }
    const size_t needed = 2 + textSize(key) + textSize(name) + textSize(unit) + 8;
    if (m_at + needed + 1 > m_space) { // +1 für die Prüfsumme
      return false;
    }
    detail::putU8(m_dst, m_at, channel);
    // Bit1 kam später dazu; ältere Daten haben dort eine Null, und das ist für
    // die damals unterstützten Sensoren genau richtig (zweiseitige Grenzen).
    detail::putU8(m_dst, m_at,
                  static_cast<uint8_t>((analog ? 0x01 : 0x00) | (oneSided ? 0x02 : 0x00)));
    putText(key);
    putText(name);
    putText(unit);
    detail::putU16(m_dst, m_at, halfFromFloat(yellowLow));
    detail::putU16(m_dst, m_at, halfFromFloat(greenLow));
    detail::putU16(m_dst, m_at, halfFromFloat(greenHigh));
    detail::putU16(m_dst, m_at, halfFromFloat(yellowHigh));
    m_count++;
    return true;
  }

  /// @return Länge des Rahmens, 0 wenn er nicht gebaut werden konnte
  size_t finish() {
    if (!m_ok || m_count == 0 || m_at + 1 > m_space) {
      return 0;
    }
    m_dst[m_countAt] = m_count;
    m_dst[m_at] = crc8(m_dst, m_at);
    return m_at + 1;
  }

  uint8_t count() const { return m_count; }

private:
  /**
   * @brief Länge eines Textes, auf MAX_TEXT gekürzt - aber nie mitten in einem
   *        UTF-8-Zeichen
   * @details Messwertnamen sind im Adminbereich frei änderbar. Ein glatter
   *          Schnitt bei 31 Byte könnte eine Mehrbytefolge zerteilen ("µg/m³",
   *          Umlaute), und der Browser bekäme kaputte Zeichen zu sehen.
   */
  static size_t textLength(const char* text) {
    if (!text) {
      return 0;
    }
    size_t len = strnlen(text, MAX_TEXT);
    if (text[len] == '\0') {
      return len; // vollständig, nichts zu kürzen
    }
    // Auf den Anfang des letzten angeschnittenen Zeichens zurückgehen:
    // Folgebytes einer UTF-8-Sequenz haben das Bitmuster 10xxxxxx.
    while (len > 0 && (static_cast<uint8_t>(text[len]) & 0xC0) == 0x80) {
      len--;
    }
    return len;
  }

  static size_t textSize(const char* text) { return 1 + textLength(text); }

  void putText(const char* text) {
    const size_t len = textLength(text);
    m_dst[m_at++] = static_cast<uint8_t>(len);
    for (size_t i = 0; i < len; i++) {
      m_dst[m_at++] = static_cast<uint8_t>(text[i]);
    }
  }

  uint8_t* m_dst;
  size_t m_space;
  size_t m_at{0};
  size_t m_countAt{0};
  uint8_t m_count{0};
  bool m_ok{true};
};

/// Ein Eintrag der Kanaltabelle, wie readTable() ihn zurückmeldet. Die Zeiger
/// zeigen in den übergebenen Puffer und sind NICHT nullterminiert - deshalb
/// die Längen daneben.
struct TableEntry {
  uint8_t channel{0};
  bool analog{false};
  /// Nur obere Grenzen ausgewertet (Feinstaub, CO2) - siehe
  /// Sensor::usesOneSidedLimits().
  bool oneSided{false};
  const char* key{nullptr};
  uint8_t keyLength{0};
  const char* name{nullptr};
  uint8_t nameLength{0};
  const char* unit{nullptr};
  uint8_t unitLength{0};
  float yellowLow{0.0f}, greenLow{0.0f}, greenHigh{0.0f}, yellowHigh{0.0f};
};

using TableEntryCallback = void (*)(void* context, const TableEntry& entry);

/**
 * @brief Kanaltabelle lesen und je Kanal zurückmelden
 * @return Anzahl gelesener Bytes, 0 wenn dort keine gültige Tabelle steht
 * @details Meldet über einen Rückruf statt in ein Feld zu schreiben, damit der
 *          Aufrufer nur das behält, was er braucht - der CSV-Export etwa nur
 *          die Schlüssel.
 */
inline size_t readTable(const uint8_t* src, size_t length, TableEntryCallback callback,
                        void* context) {
  if (!src || length < 8 || detail::getU16(src) != MAGIC_TABLE) {
    return 0;
  }
  const uint8_t count = src[6];
  if (count > MAX_CHANNELS) {
    return 0;
  }

  // Erst vollständig prüfen, dann melden: sonst bekäme der Aufrufer Einträge
  // aus einem Rahmen, der sich hinterher als beschädigt herausstellt.
  size_t at = 7;
  for (uint8_t i = 0; i < count; i++) {
    if (at + 2 > length)
      return 0;
    at += 2;
    for (uint8_t t = 0; t < 3; t++) {
      if (at >= length)
        return 0;
      const uint8_t len = src[at++];
      if (len > MAX_TEXT || at + len > length)
        return 0;
      at += len;
    }
    if (at + 8 > length)
      return 0;
    at += 8;
  }
  if (at >= length || src[at] != crc8(src, at)) {
    return 0;
  }
  const size_t total = at + 1;

  if (!callback) {
    return total;
  }

  at = 7;
  for (uint8_t i = 0; i < count; i++) {
    TableEntry entry;
    entry.channel = src[at++];
    const uint8_t flags = src[at++];
    entry.analog = (flags & 0x01) != 0;
    entry.oneSided = (flags & 0x02) != 0;

    const char** texts[3] = {&entry.key, &entry.name, &entry.unit};
    uint8_t* lengths[3] = {&entry.keyLength, &entry.nameLength, &entry.unitLength};
    for (uint8_t t = 0; t < 3; t++) {
      const uint8_t len = src[at++];
      *texts[t] = reinterpret_cast<const char*>(src + at);
      *lengths[t] = len;
      at += len;
    }

    float* limits[4] = {&entry.yellowLow, &entry.greenLow, &entry.greenHigh, &entry.yellowHigh};
    for (uint8_t l = 0; l < 4; l++) {
      *limits[l] = floatFromHalf(detail::getU16(src + at));
      at += 2;
    }
    callback(context, entry);
  }
  return total;
}

/**
 * @brief Länge eines gültigen Rahmens beliebigen Typs
 * @return Länge oder 0, wenn dort kein vollständiger, prüfsummenrichtiger
 *         Rahmen steht
 * @details Prüft, ohne den Inhalt zu entpacken - genau das braucht der
 *          Startscan, der nur Rahmengrenzen sucht.
 */
inline size_t frameLength(const uint8_t* src, size_t length) {
  if (!src || length < 2) {
    return 0;
  }
  const uint16_t magic = detail::getU16(src);
  if (magic == MAGIC_SAMPLE) {
    SampleFrame frame;
    return readSample(src, length, frame);
  }
  if (magic == MAGIC_TABLE) {
    return readTable(src, length, nullptr, nullptr);
  }
  return 0;
}

/**
 * @brief Nächstes Magic ab Position from suchen
 * @return Position des Magic oder length, wenn keines mehr folgt
 * @details Damit setzt ein Leser nach einer Störstelle wieder auf, statt den
 *          Rest des Segments zu verlieren. Ein Magic, das über das Pufferende
 *          hinausragt, gilt nicht als gefunden.
 */
inline size_t findMagic(const uint8_t* src, size_t length, size_t from) {
  if (!src || length < 2) {
    return length;
  }
  for (size_t i = from; i + 1 < length; i++) {
    const uint16_t magic = detail::getU16(src + i);
    if (magic == MAGIC_SAMPLE || magic == MAGIC_TABLE) {
      return i;
    }
  }
  return length;
}

/**
 * @brief Länge des vollständig gültigen Anfangs eines Segments
 * @details Beim Start wird das jüngste Segment damit geprüft: ist die gültige
 *          Länge kleiner als die Dateigröße, wurde beim letzten Stromausfall
 *          ein Rahmen angeschnitten und die Datei wird auf diese Länge
 *          gekürzt. Nur so ist die nächste Anfügung wieder eindeutig.
 */
inline size_t validPrefixLength(const uint8_t* src, size_t length) {
  size_t at = 0;
  while (at < length) {
    const size_t read = frameLength(src + at, length - at);
    if (read == 0) {
      break;
    }
    at += read; // frameLength liefert nie 0 Fortschritt, die Schleife terminiert
  }
  return at;
}

} // namespace ChronikFormat

#endif // CHRONIK_FORMAT_H
