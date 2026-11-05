/**
 * @file crypto.h
 * @brief AES-256-GCM für den Mailbot (ESP8266: BearSSL, ESP32: mbedTLS,
 *        nativ: OpenSSL)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {
namespace hal {

/// Verschlüsselt buf in place und liefert den 16-Byte-Tag
bool aes256gcmSeal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t* aad,
                   size_t aadLen, uint8_t* buf, size_t len, uint8_t tag[16]);

} // namespace hal
} // namespace ps
