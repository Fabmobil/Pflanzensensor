/**
 * @file rtc_mem.h
 * @brief Speicher, der einen Software-Neustart übersteht (RTC-RAM / .noinit)
 * @details ESP8266: system_rtc_mem_*; ESP32: RTC_NOINIT_ATTR. Nach Power-on
 *          ist der Inhalt zufällig, deshalb mit Magic und Prüfsumme.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {
namespace hal {

/// Höchstens 64 Byte
bool rtcRead(void* data, size_t len);
bool rtcWrite(const void* data, size_t len);

} // namespace hal
} // namespace ps
