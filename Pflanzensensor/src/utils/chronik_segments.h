/**
 * @file chronik_segments.h
 * @brief Namen und Reihenfolge der Chronik-Segmentdateien
 * @details Header-only und hardwarefrei (siehe chronik_format.h).
 *
 *          Die Segmente liegen im Wurzelverzeichnis, nicht in einem eigenen
 *          Ordner: jedes Verzeichnis kostet auf LittleFS ein
 *          Metadaten-Blockpaar, also 16384 B - bei einem Budget von rund
 *          160 KB wären das zwei Segmente allein für den Ordnernamen. Die
 *          Verzeichniseinträge selbst (~40 B je Datei) passen ins ohnehin
 *          vorhandene Blockpaar der Wurzel.
 *
 *          Der Index im Dateinamen steigt monoton und wird nie zurückgesetzt.
 *          Damit ist "das jüngste Segment" allein am Namen erkennbar, ohne
 *          eine einzige Datei zu öffnen, und es gibt keinen Sonderfall beim
 *          Überlauf einer Ringnummerierung.
 */

#ifndef CHRONIK_SEGMENTS_H
#define CHRONIK_SEGMENTS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace ChronikSegments {

/// "/chr_0000002a.dat" - 17 Zeichen plus Abschluss, LFS_NAME_MAX ist 32.
static constexpr const char* PREFIX = "/chr_";
static constexpr const char* SUFFIX = ".dat";
static constexpr size_t NAME_LENGTH = 17;
static constexpr size_t NAME_BUFFER = NAME_LENGTH + 1;

/**
 * @brief Dateinamen zu einem Index bilden
 * @param out Puffer mit mindestens NAME_BUFFER Bytes
 * @details Feste Breite von acht Hexstellen, damit die Namen auch alphabetisch
 *          in der richtigen Reihenfolge stehen - das erleichtert die
 *          Fehlersuche in einem Verzeichnislisting.
 */
inline void nameFromIndex(uint32_t index, char* out) {
  if (!out) {
    return;
  }
  // Nicht HEX nennen: Arduinos Print.h definiert HEX als Makro für die Zahl 16,
  // der Name wäre hier zu 16[...] expandiert.
  static const char HEXDIGITS[] = "0123456789abcdef";
  memcpy(out, PREFIX, 5);
  for (uint8_t i = 0; i < 8; i++) {
    out[5 + i] = HEXDIGITS[(index >> ((7 - i) * 4)) & 0x0F];
  }
  memcpy(out + 13, SUFFIX, 4);
  out[NAME_LENGTH] = '\0';
}

/**
 * @brief Index aus einem Dateinamen lesen
 * @return Index oder -1, wenn der Name kein Chronik-Segment ist
 * @details Nimmt den Namen mit und ohne führenden Schrägstrich entgegen: die
 *          Verzeichnisiteration liefert je nach Aufrufweg das eine oder das
 *          andere, und ein hier übersehenes Segment würde nie gelöscht.
 */
inline int64_t indexFromName(const char* name) {
  if (!name) {
    return -1;
  }
  const char* p = (name[0] == '/') ? name + 1 : name;
  if (strncmp(p, PREFIX + 1, 4) != 0) { // "chr_"
    return -1;
  }
  p += 4;

  uint32_t index = 0;
  for (uint8_t i = 0; i < 8; i++) {
    const char c = p[i];
    uint32_t digit;
    if (c >= '0' && c <= '9') {
      digit = static_cast<uint32_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      digit = static_cast<uint32_t>(c - 'a' + 10);
    } else {
      return -1;
    }
    index = (index << 4) | digit;
  }
  if (strcmp(p + 8, SUFFIX) != 0) {
    return -1;
  }
  return static_cast<int64_t>(index);
}

/**
 * @brief Merkt sich beim Verzeichnisdurchlauf ältestes und jüngstes Segment
 * @details Bewusst ohne Liste: der ChronikStore braucht nur die beiden Ränder
 *          und die Anzahl, und ein Array von 28 Indizes wäre auf einem Gerät
 *          mit 16 KB freiem Heap unnötiger Ballast.
 */
class SegmentRange {
public:
  void add(uint32_t index) {
    if (m_count == 0) {
      m_oldest = index;
      m_newest = index;
    } else {
      if (index < m_oldest)
        m_oldest = index;
      if (index > m_newest)
        m_newest = index;
    }
    m_count++;
  }

  bool empty() const { return m_count == 0; }
  uint16_t count() const { return m_count; }
  uint32_t oldest() const { return m_oldest; }
  uint32_t newest() const { return m_newest; }
  /// Index, den das nächste anzulegende Segment bekommt.
  uint32_t nextIndex() const { return m_count == 0 ? 0 : m_newest + 1; }

  void reset() {
    m_count = 0;
    m_oldest = 0;
    m_newest = 0;
  }

private:
  uint16_t m_count{0};
  uint32_t m_oldest{0};
  uint32_t m_newest{0};
};

} // namespace ChronikSegments

#endif // CHRONIK_SEGMENTS_H
