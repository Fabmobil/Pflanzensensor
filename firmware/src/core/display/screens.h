/**
 * @file screens.h
 * @brief Bildschirme als reine Funktionen (refactor.md 2.7, 5.14)
 */
#pragma once

#include "core/display/canvas.h"

namespace ps {
namespace screens {

/// Info: <host>.local (bis 2 Zeilen), IP, SSID
void info(Canvas& c, const char* host, const char* ip, const char* ssid);
/// Uhr: HH:MM groß, Datum klein; ampm: nullptr (24h) oder "AM"/"PM"
void clock(Canvas& c, uint8_t hour, uint8_t minute, uint8_t day, uint8_t month, uint16_t year,
           const char* ampm);
/// QR-Code zu url (Version 2/3, ECC low, Skalierung 2, unten bündig)
bool qr(Canvas& c, const char* url);
void flower(Canvas& c);
void fabmobil(Canvas& c);
/// Kanal: Name klein, Wert (1 Nachkommastelle) + Einheit groß, Linien
void channel(Canvas& c, const char* name, float value, const char* unit);
/// Bootlog: Kopf + bis zu 6 Zeilen
void bootLog(Canvas& c, const char* header, const char* const* lines, uint8_t n);
/// OTA-Fortschritt
void progress(Canvas& c, const char* title, uint8_t percent);

} // namespace screens
} // namespace ps
