/**
 * @file system.cpp
 * @brief hal/system.h, hal/rtc_mem.h und hal/crypto.h für ESP8266 und ESP32
 */
#if defined(ARDUINO)

#include <Arduino.h>
#include <time.h>

#include "hal/crypto.h"
#include "hal/rtc_mem.h"
#include "hal/system.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <bearssl/bearssl_aead.h>
#include <bearssl/bearssl_block.h>
#include <user_interface.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_system.h>
#include <mbedtls/gcm.h>
#endif

namespace ps {
namespace hal {

Instant now() { return Instant::fromMillis(millis()); }

uint32_t epoch() {
  const time_t t = time(nullptr);
  return t > 0 ? static_cast<uint32_t>(t) : 0;
}

bool toLocalTime(uint32_t ep, LocalTime& out) {
  if (!epochValid(ep))
    return false;
  const time_t t = static_cast<time_t>(ep);
  struct tm tmv;
  localtime_r(&t, &tmv);
  out.year = static_cast<uint16_t>(tmv.tm_year + 1900);
  out.month = static_cast<uint8_t>(tmv.tm_mon + 1);
  out.day = static_cast<uint8_t>(tmv.tm_mday);
  out.hour = static_cast<uint8_t>(tmv.tm_hour);
  out.minute = static_cast<uint8_t>(tmv.tm_min);
  out.second = static_cast<uint8_t>(tmv.tm_sec);
  return true;
}

void timeBegin(const char* tz, const char* server) {
#if defined(ARDUINO_ARCH_ESP8266)
  configTime(tz, server);
#else
  configTzTime(tz, server);
#endif
}

void yieldNow() { yield(); }

void restart() { ESP.restart(); }

void fillRandom(uint8_t* buf, size_t len) {
#if defined(ARDUINO_ARCH_ESP8266)
  ESP.random(buf, len); // Hardware-RNG
#else
  esp_fill_random(buf, len);
#endif
}

HeapInfo heap() {
  HeapInfo h;
  h.free = ESP.getFreeHeap();
#if defined(ARDUINO_ARCH_ESP8266)
  h.maxBlock = ESP.getMaxFreeBlockSize();
  h.fragmentation = ESP.getHeapFragmentation();
#else
  h.maxBlock = ESP.getMaxAllocHeap();
  h.fragmentation = h.free ? static_cast<uint8_t>(100 - (h.maxBlock * 100 / h.free)) : 0;
#endif
  return h;
}

void macAddress(uint8_t mac[6]) { WiFi.macAddress(mac); }

void macString(char out[18]) {
  uint8_t m[6];
  macAddress(m);
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

ResetReason resetReason() {
#if defined(ARDUINO_ARCH_ESP8266)
  const rst_info* r = ESP.getResetInfoPtr();
  switch (r ? r->reason : 0) {
  case REASON_DEFAULT_RST:
    return ResetReason::PowerOn;
  case REASON_WDT_RST:
  case REASON_SOFT_WDT_RST:
    return ResetReason::Watchdog;
  case REASON_EXCEPTION_RST:
    return ResetReason::Exception;
  case REASON_SOFT_RESTART:
    return ResetReason::Software;
  case REASON_DEEP_SLEEP_AWAKE:
    return ResetReason::DeepSleep;
  case REASON_EXT_SYS_RST:
    return ResetReason::External;
  default:
    return ResetReason::Unknown;
  }
#else
  switch (esp_reset_reason()) {
  case ESP_RST_POWERON:
    return ResetReason::PowerOn;
  case ESP_RST_EXT:
    return ResetReason::External;
  case ESP_RST_SW:
    return ResetReason::Software;
  case ESP_RST_PANIC:
    return ResetReason::Exception;
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
    return ResetReason::Watchdog;
  case ESP_RST_DEEPSLEEP:
    return ResetReason::DeepSleep;
  case ESP_RST_BROWNOUT:
    return ResetReason::Brownout;
  default:
    return ResetReason::Unknown;
  }
#endif
}

uint32_t freeSketchSpace() { return ESP.getFreeSketchSpace(); }

void consoleBegin(uint32_t baud) { Serial.begin(baud); }

#if defined(ARDUINO_ARCH_ESP8266)
namespace {
/// Ein freier Sektor hinter LittleFS (_FS_end) und vor dem EEPROM-Sektor,
/// siehe boards/eagle.flash.4mmore.ld
constexpr uint32_t kProvisionAddr = 0x3FA000;
} // namespace

bool provisionRead(uint8_t* buf, size_t len) {
  return len % 4 == 0 && ESP.flashRead(kProvisionAddr, reinterpret_cast<uint32_t*>(buf), len);
}

bool provisionWrite(const uint8_t* buf, size_t len) {
  if (len % 4 != 0 || len > 4096)
    return false;
  if (!ESP.flashEraseSector(kProvisionAddr / 4096))
    return false;
  return ESP.flashWrite(kProvisionAddr, reinterpret_cast<const uint32_t*>(buf), len);
}
#else
namespace {
const esp_partition_t* provisionPartition() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "provision");
}
} // namespace

bool provisionRead(uint8_t* buf, size_t len) {
  const esp_partition_t* p = provisionPartition();
  return p && len <= p->size && esp_partition_read(p, 0, buf, len) == ESP_OK;
}

bool provisionWrite(const uint8_t* buf, size_t len) {
  const esp_partition_t* p = provisionPartition();
  if (!p || len > p->size)
    return false;
  return esp_partition_erase_range(p, 0, p->size) == ESP_OK &&
         esp_partition_write(p, 0, buf, len) == ESP_OK;
}
#endif

size_t consoleRead(uint8_t* buf, size_t cap) {
  size_t n = 0;
  while (n < cap && Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0)
      break;
    buf[n++] = static_cast<uint8_t>(c);
  }
  return n;
}

void consoleWrite(const char* data, size_t len) {
  Serial.write(reinterpret_cast<const uint8_t*>(data), len);
}

void* tempAlloc(size_t n) { return malloc(n); }
void tempFree(void* p) { free(p); }

// ---------------------------------------------------------------------------
// RTC-RAM
// ---------------------------------------------------------------------------
#if defined(ARDUINO_ARCH_ESP8266)
namespace {
// Anwenderbereich beginnt bei Block 64 (0x60001200). Dessen erste 128 Byte
// (Blöcke 64..95) gehören dem Bootloader: eboot legt dort das OTA-Kommando
// ab (eboot_command.h, RTC_MEM 0x60001200). Der Boot-Schutz lag genau dort
// und zählte nach einem OTA-Start falsch weiter. Daher Block 128.
constexpr uint32_t kRtcBlock = 128;
} // namespace
bool rtcRead(void* data, size_t len) {
  if (len > 64 || len % 4)
    return false;
  return system_rtc_mem_read(kRtcBlock, data, len);
}
bool rtcWrite(const void* data, size_t len) {
  if (len > 64 || len % 4)
    return false;
  return system_rtc_mem_write(kRtcBlock, const_cast<void*>(data), len);
}
#else
namespace {
RTC_NOINIT_ATTR uint8_t g_rtc[64];
}
bool rtcRead(void* data, size_t len) {
  if (len > sizeof(g_rtc))
    return false;
  memcpy(data, g_rtc, len);
  return true;
}
bool rtcWrite(const void* data, size_t len) {
  if (len > sizeof(g_rtc))
    return false;
  memcpy(g_rtc, data, len);
  return true;
}
#endif

// ---------------------------------------------------------------------------
// AES-256-GCM
// ---------------------------------------------------------------------------
bool aes256gcmSeal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t* aad,
                   size_t aadLen, uint8_t* buf, size_t len, uint8_t tag[16]) {
#if defined(ARDUINO_ARCH_ESP8266)
  // Aufrufreihenfolge wie im Altcode (mail_crypto.cpp), gegen den Testvektor geprüft
  br_aes_ct_ctr_keys block;
  br_aes_ct_ctr_init(&block, key, 32);
  br_gcm_context gcm;
  br_gcm_init(&gcm, &block.vtable, br_ghash_ctmul32);
  br_gcm_reset(&gcm, nonce, 12);
  br_gcm_aad_inject(&gcm, aad, aadLen);
  br_gcm_flip(&gcm);
  br_gcm_run(&gcm, 1, buf, len);
  br_gcm_get_tag(&gcm, tag);
  return true;
#else
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  bool ok = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
            mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, len, nonce, 12, aad, aadLen, buf,
                                      buf, 16, tag) == 0;
  mbedtls_gcm_free(&ctx);
  return ok;
#endif
}

} // namespace hal
} // namespace ps

#endif // ARDUINO
