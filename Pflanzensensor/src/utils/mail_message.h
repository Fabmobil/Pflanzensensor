/**
 * @file mail_message.h
 * @brief Kopfzeilen einer Mail bauen - ohne Hardware, damit nativ testbar
 * @details Header-only wie utils/smtp_session.h. Hier stecken die Formate, die
 *          eine Mail überhaupt erst zustellbar machen und die man beim
 *          Ausprobieren schwer prüft: ein Datum nach RFC 5322 (immer englische
 *          Abkürzungen, unabhängig von der Spracheinstellung) und ein Betreff
 *          mit Umlauten nach RFC 2047 - ohne den zeigen viele Programme
 *          "Bodenfeuchte kritisch" als "Bodenfeuchte kritisch" mit
 *          zerschossenen Zeichen an oder Spamfilter werden misstrauisch.
 */

#ifndef MAIL_MESSAGE_H
#define MAIL_MESSAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "utils/smtp_session.h"

namespace Mail {

/**
 * @brief Datum im Format von RFC 5322: "Mon, 31 Aug 2026 14:54:36 +0000"
 * @return Länge ohne Nullbyte, 0 bei zu kleinem Puffer oder unplausibler Zeit
 * @details Bewusst UTC mit "+0000" statt Ortszeit: die Zeitzone des Geräts
 *          hängt an einer Umgebungsvariablen, und ein falscher Versatz lässt
 *          Mails in der falschen Reihenfolge einsortiert erscheinen. Die
 *          Wochentags- und Monatsnamen sind laut Norm englisch und dürfen
 *          nicht aus der Lokalisierung kommen.
 */
inline size_t formatDate(time_t when, char* out, size_t outSize) {
  static const char* TAGE[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* MONATE[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (!out || outSize < 32 || when < 1000000000) {
    return 0;
  }
  struct tm zeit;
  gmtime_r(&when, &zeit);
  if (zeit.tm_wday < 0 || zeit.tm_wday > 6 || zeit.tm_mon < 0 || zeit.tm_mon > 11) {
    return 0;
  }
  const int n = snprintf(out, outSize, "%s, %02d %s %04d %02d:%02d:%02d +0000", TAGE[zeit.tm_wday],
                         zeit.tm_mday, MONATE[zeit.tm_mon], zeit.tm_year + 1900, zeit.tm_hour,
                         zeit.tm_min, zeit.tm_sec);
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

/// @brief Besteht der Text nur aus druckbarem ASCII?
inline bool isPlainAscii(const char* text) {
  if (!text) {
    return true;
  }
  for (const char* p = text; *p; p++) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x20 || c > 0x7E) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Betreff kopieren, bei Nicht-ASCII nach RFC 2047 kodieren
 * @return Länge ohne Nullbyte, 0 bei zu kleinem Puffer
 * @details "=?UTF-8?B?...?=" - Base64 statt Quoted-Printable, weil der Kodierer
 *          dafür schon da ist (AUTH LOGIN) und deutsche Betreffzeilen ohnehin
 *          reichlich Umlaute haben.
 */
inline size_t encodeSubject(const char* subject, char* out, size_t outSize) {
  if (!subject || !out) {
    return 0;
  }
  const size_t length = strlen(subject);

  if (isPlainAscii(subject)) {
    if (length + 1 > outSize) {
      return 0;
    }
    memcpy(out, subject, length + 1);
    return length;
  }

  const char* PRAEFIX = "=?UTF-8?B?";
  const char* SUFFIX = "?=";
  const size_t b64Laenge = ((length + 2) / 3) * 4;
  const size_t noetig = strlen(PRAEFIX) + b64Laenge + strlen(SUFFIX);
  if (noetig + 1 > outSize) {
    return 0;
  }

  size_t at = strlen(PRAEFIX);
  memcpy(out, PRAEFIX, at);
  if (Smtp::base64Encode(subject, length, out + at, outSize - at) == 0) {
    return 0;
  }
  at += b64Laenge;
  memcpy(out + at, SUFFIX, strlen(SUFFIX) + 1);
  return at + strlen(SUFFIX);
}

/**
 * @brief Message-ID bauen
 * @details Ohne sie vergeben manche Server eine eigene, andere werten das
 *          Fehlen als Spamhinweis. Die Kennung muss weltweit eindeutig sein,
 *          deshalb Zeitstempel plus Zähler plus Gerätename.
 */
inline size_t formatMessageId(const char* device, const char* domain, time_t when, uint32_t counter,
                              char* out, size_t outSize) {
  if (!out || !domain) {
    return 0;
  }
  const int n = snprintf(out, outSize, "<%lu.%lu.%s@%s>", static_cast<unsigned long>(when),
                         static_cast<unsigned long>(counter), device ? device : "sensor", domain);
  return (n > 0 && static_cast<size_t>(n) < outSize) ? static_cast<size_t>(n) : 0;
}

/**
 * @brief Domänenteil einer Adresse ("a@b.c" -> "b.c")
 * @details Für die Message-ID: sie soll aus der Absenderdomäne stammen, sonst
 *          fällt sie manchen Prüfungen negativ auf.
 */
inline size_t domainOf(const char* address, char* out, size_t outSize) {
  if (!address || !out) {
    return 0;
  }
  const char* at = strrchr(address, '@');
  if (!at || !at[1]) {
    return 0;
  }
  const size_t length = strlen(at + 1);
  if (length + 1 > outSize) {
    return 0;
  }
  memcpy(out, at + 1, length + 1);
  return length;
}

/**
 * @brief Sieht das nach einer brauchbaren Mailadresse aus?
 * @details Keine vollständige Prüfung nach RFC 5322 - die will niemand haben.
 *          Es geht darum, Tippfehler in der Weboberfläche abzufangen, bevor
 *          der Server die Nachricht mit 550 ablehnt: genau ein @, links und
 *          rechts etwas, rechts mindestens ein Punkt, keine Leerzeichen.
 */
inline bool looksLikeAddress(const char* address) {
  if (!address || !*address) {
    return false;
  }
  const char* at = strchr(address, '@');
  if (!at || at == address || strchr(at + 1, '@')) {
    return false;
  }
  const char* punkt = strchr(at + 1, '.');
  if (!punkt || punkt == at + 1 || !punkt[1]) {
    return false;
  }
  for (const char* p = address; *p; p++) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c <= 0x20 || c == 0x7F || c == ',' || c == ';' || c == '<' || c == '>') {
      return false;
    }
  }
  return true;
}

} // namespace Mail

#endif // MAIL_MESSAGE_H
