/**
 * @file base64.h
 * @brief Minimale Base64-Kodierung/Dekodierung - reine, hardwareunabhängige
 *        Implementierung
 * @details Eigene Datei statt einer Bibliotheksabhängigkeit, weil der
 *          Algorithmus an nichts Hardwarespezifischem hängt und die
 *          Mail-Funktion (Preferences-Speicherung des Geräte-Schlüssels,
 *          HTTP-JSON-Umschlag) ihn an mehreren unabhängigen Stellen braucht
 *          (managers/, mail/). decode() schreibt in einen vom Aufrufer
 *          bereitgestellten Puffer fester Größe statt zu allozieren - passt
 *          zu den durchweg festen, kleinen Puffergrößen (Schlüssel: 32 Byte,
 *          Nonce: 12 Byte, Tag: 16 Byte) und vermeidet Heap-Fragmentierung.
 */

#ifndef BASE64_H
#define BASE64_H

#include <Arduino.h>

namespace Base64 {

static const char ENCODE_TABLE[] PROGMEM =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Kodiert Rohbytes als Base64-String (mit '='-Padding)
 */
inline String encode(const uint8_t* data, size_t len) {
  String out;
  out.reserve(((len + 2) / 3) * 4);

  size_t i = 0;
  while (i + 3 <= len) {
    uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                 (static_cast<uint32_t>(data[i + 1]) << 8) | data[i + 2];
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 18) & 0x3F]));
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 12) & 0x3F]));
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 6) & 0x3F]));
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[n & 0x3F]));
    i += 3;
  }

  size_t remaining = len - i;
  if (remaining == 1) {
    uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 18) & 0x3F]));
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 12) & 0x3F]));
    out += "==";
  } else if (remaining == 2) {
    uint32_t n = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8);
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 18) & 0x3F]));
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 12) & 0x3F]));
    out += static_cast<char>(pgm_read_byte(&ENCODE_TABLE[(n >> 6) & 0x3F]));
    out += "=";
  }

  return out;
}

/**
 * @brief Wandelt ein Base64-Zeichen in seinen 6-Bit-Wert um, -1 wenn ungültig
 */
inline int decodeChar(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

/**
 * @brief Dekodiert einen Base64-String in einen vom Aufrufer bereitgestellten
 *        Puffer
 * @param in Base64-kodierter String (Padding optional)
 * @param out Zielpuffer
 * @param outCapacity Größe von out in Byte
 * @return Anzahl geschriebener Bytes, oder -1 bei ungültiger Eingabe oder zu
 *         kleinem Zielpuffer
 */
inline int decode(const String& in, uint8_t* out, size_t outCapacity) {
  // Padding und Whitespace am Ende ignorieren
  int inLen = in.length();
  while (inLen > 0 && (in[inLen - 1] == '=' || in[inLen - 1] == '\n' || in[inLen - 1] == '\r'))
    inLen--;

  size_t outLen = 0;
  int buffer = 0;
  int bits = 0;

  for (int i = 0; i < inLen; ++i) {
    int val = decodeChar(in[i]);
    if (val < 0)
      return -1; // ungültiges Zeichen

    buffer = (buffer << 6) | val;
    bits += 6;

    if (bits >= 8) {
      bits -= 8;
      if (outLen >= outCapacity)
        return -1; // Zielpuffer zu klein
      out[outLen++] = static_cast<uint8_t>((buffer >> bits) & 0xFF);
    }
  }

  return static_cast<int>(outLen);
}

} // namespace Base64

#endif // BASE64_H
