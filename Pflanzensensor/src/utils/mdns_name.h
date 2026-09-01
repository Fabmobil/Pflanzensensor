/**
 * @file mdns_name.h
 * @brief Aus dem Gerätenamen einen gültigen mDNS-Hostnamen machen
 * @details Header-only und ohne Arduino, wie utils/mail_scheduler.h - so lässt
 *          sich die Umformung mit `pio test -e native` prüfen, ohne Hardware.
 *
 *          Der Gerätename ist frei wählbar ("Frameclaw PS", "Balkon Müller"),
 *          ein Hostname ist es nicht: RFC 1035 erlaubt je Namensteil nur
 *          Buchstaben, Ziffern und Bindestriche, höchstens 63 Zeichen, und
 *          nicht mit einem Bindestrich beginnen oder enden. Ein Leerzeichen
 *          oder Umlaut im Namen macht den Sensor sonst unerreichbar - und
 *          zwar stillschweigend, denn MDNS.begin() meldet das nicht.
 */

#ifndef MDNS_NAME_H
#define MDNS_NAME_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace MdnsName {

/// RFC 1035: ein Namensteil darf höchstens 63 Zeichen lang sein.
constexpr size_t MAX_LEN = 63;

/// Wenn vom Gerätenamen nichts Brauchbares übrig bleibt.
constexpr const char* RUECKFALL = "pflanzensensor";

/**
 * @brief Deutsche Umlaute ausschreiben
 * @param a erstes Byte der UTF-8-Folge, @param b zweites
 * @return Ersatztext oder nullptr, wenn es kein bekannter Umlaut ist
 * @details Ohne das würde aus "Grün" ein "gr-n" - lesbar ist anders. Die
 *          Umlaute sind in UTF-8 zwei Byte lang (C3 A4 für ä und so weiter).
 */
inline const char* umlaut(unsigned char a, unsigned char b) {
  if (a != 0xC3) {
    return nullptr;
  }
  switch (b) {
  case 0xA4: // ä
  case 0x84: // Ä
    return "ae";
  case 0xB6: // ö
  case 0x96: // Ö
    return "oe";
  case 0xBC: // ü
  case 0x9C: // Ü
    return "ue";
  case 0x9F: // ß
    return "ss";
  default:
    return nullptr;
  }
}

inline bool istErlaubt(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

/**
 * @brief Gerätenamen in einen Hostnamen umformen
 * @return Länge ohne Nullbyte
 * @details Kleinbuchstaben, Umlaute ausgeschrieben, alles Übrige wird zum
 *          Bindestrich; mehrere Bindestriche werden zu einem, vorn und hinten
 *          fallen sie weg. Aus "Frameclaw PS" wird "frameclaw-ps", der Sensor
 *          heißt dann frameclaw-ps.local.
 */
inline size_t hostnameVon(const char* geraetename, char* out, size_t outSize) {
  if (!out || outSize == 0) {
    return 0;
  }
  const size_t platz = (outSize - 1 < MAX_LEN) ? outSize - 1 : MAX_LEN;
  size_t at = 0;

  auto anhaengen = [&](char c) {
    // Bindestriche nie doppelt und nie am Anfang
    if (c == '-' && (at == 0 || out[at - 1] == '-')) {
      return;
    }
    if (at < platz) {
      out[at++] = c;
    }
  };

  for (const char* p = geraetename ? geraetename : ""; *p; p++) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c >= 0x80) {
      const char* ersatz = umlaut(c, static_cast<unsigned char>(p[1]));
      if (ersatz) {
        for (const char* e = ersatz; *e; e++) {
          anhaengen(*e);
        }
        p++; // zweites Byte der Folge überspringen
      } else {
        anhaengen('-');
        // Folgebytes der UTF-8-Sequenz mitnehmen, sonst gäbe jedes einen
        // eigenen Bindestrich
        while ((static_cast<unsigned char>(p[1]) & 0xC0) == 0x80) {
          p++;
        }
      }
      continue;
    }
    if (c >= 'A' && c <= 'Z') {
      anhaengen(static_cast<char>(c - 'A' + 'a'));
    } else if (istErlaubt(static_cast<char>(c))) {
      anhaengen(static_cast<char>(c));
    } else {
      anhaengen('-');
    }
  }

  // Hinten dürfen keine Bindestriche stehen
  while (at > 0 && out[at - 1] == '-') {
    at--;
  }
  out[at] = '\0';

  if (at == 0) {
    strncpy(out, RUECKFALL, outSize - 1);
    out[outSize - 1] = '\0';
    return strlen(out);
  }
  return at;
}

} // namespace MdnsName

#endif // MDNS_NAME_H
