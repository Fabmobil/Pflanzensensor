/**
 * @file native.cpp
 * @brief hal-Funktionen für den nativen Build
 */
#if !defined(ARDUINO)

#include "platform/native/native.h"

#include <openssl/evp.h>
#include <stdlib.h>
#include <time.h>

#include <random>

#include "hal/crypto.h"
#include "hal/rtc_mem.h"

namespace ps {
namespace native {
namespace {
uint32_t g_ms = 0;
uint32_t g_epoch = 0;
hal::ResetReason g_reason = hal::ResetReason::PowerOn;
uint8_t g_rtc[64];
bool g_rtcValid = false;
} // namespace

void setMillis(uint32_t ms) { g_ms = ms; }
/// Nur millis(); die Epoche setzen Tests ausdrücklich mit setEpoch()
void advance(uint32_t ms) { g_ms += ms; }
void setEpoch(uint32_t e) { g_epoch = e; }
void setResetReason(hal::ResetReason r) { g_reason = r; }
std::string& console() {
  static std::string s;
  return s;
}
std::string& consoleInput() {
  static std::string s;
  return s;
}

hal::File* MemFs::open(const char* path, hal::OpenMode mode) {
  if ((mode == hal::OpenMode::Read || mode == hal::OpenMode::ReadWrite) && !files_.count(path))
    return nullptr;
  for (auto& s : slots_) {
    if (s.used)
      continue;
    auto& v = files_[path];
    if (mode == hal::OpenMode::Write)
      v.clear();
    s.data = &v;
    s.fs = this;
    s.pos = mode == hal::OpenMode::Append ? static_cast<uint32_t>(v.size()) : 0;
    s.used = true;
    s.resetFailed();
    return &s;
  }
  return nullptr;
}

void MemFs::close(hal::File* f) {
  for (auto& s : slots_)
    if (&s == f)
      s.used = false;
}

bool MemFs::rename(const char* from, const char* to) {
  auto it = files_.find(from);
  if (it == files_.end())
    return false;
  std::vector<uint8_t> v = std::move(it->second);
  files_.erase(it);
  files_[to] = std::move(v);
  return true;
}

bool MemFs::info(hal::FsInfo& out) {
  uint32_t used = 0;
  for (auto& kv : files_)
    used += static_cast<uint32_t>(((kv.second.size() + 8191) / 8192) * 8192);
  out.total = total;
  out.used = used;
  return true;
}

void MemFs::list(hal::ListCallback cb, void* ctx) {
  std::vector<std::pair<std::string, uint32_t>> copy;
  for (auto& kv : files_)
    copy.emplace_back(kv.first, static_cast<uint32_t>(kv.second.size()));
  for (auto& e : copy)
    cb(ctx, e.first.c_str(), e.second);
}

void MemFs::listRoot(hal::EntryCallback cb, void* ctx) {
  std::vector<std::pair<std::string, bool>> entries;
  for (auto& kv : files_) {
    const std::string& n = kv.first;
    const size_t slash = n.find('/', 1);
    const std::string top = slash == std::string::npos ? n : n.substr(0, slash);
    const bool dir = slash != std::string::npos;
    bool seen = false;
    for (auto& e : entries)
      seen = seen || e.first == top;
    if (!seen)
      entries.emplace_back(top, dir);
  }
  for (auto& e : entries)
    cb(ctx, e.first.c_str(), e.second);
}

bool MemFs::removeTree(const char* path) {
  const std::string p(path);
  bool any = files_.erase(p) > 0;
  for (auto it = files_.begin(); it != files_.end();) {
    if (it->first.compare(0, p.size() + 1, p + "/") == 0) {
      it = files_.erase(it);
      any = true;
    } else {
      ++it;
    }
  }
  return any;
}

size_t MemFs::Handle::read(uint8_t* buf, size_t len) {
  if (pos >= data->size())
    return 0;
  size_t n = data->size() - pos;
  if (n > len)
    n = len;
  memcpy(buf, data->data() + pos, n);
  pos += static_cast<uint32_t>(n);
  return n;
}

size_t MemFs::Handle::writeBytes(const uint8_t* buf, size_t len) {
  if (len == 0)
    return 0;
  hal::FsInfo i;
  fs->info(i);
  if (i.used + len > fs->total && data->size() + len > ((data->size() + 8191) / 8192) * 8192)
    return 0;
  if (pos + len > data->size())
    data->resize(pos + len);
  memcpy(data->data() + pos, buf, len);
  pos += static_cast<uint32_t>(len);
  return len;
}

bool MemFs::Handle::seek(uint32_t p) {
  if (p > data->size())
    return false;
  pos = p;
  return true;
}

bool MemFs::Handle::truncate(uint32_t len) {
  if (len > data->size())
    return false;
  data->resize(len);
  if (pos > len)
    pos = len;
  return true;
}

} // namespace native

namespace hal {

Instant now() { return Instant::fromMillis(native::g_ms); }
uint32_t epoch() { return native::g_epoch; }

bool toLocalTime(uint32_t ep, LocalTime& out) {
  if (!epochValid(ep))
    return false;
  // Tests rechnen in UTC
  const time_t t = static_cast<time_t>(ep);
  struct tm tmv;
  gmtime_r(&t, &tmv);
  out.year = static_cast<uint16_t>(tmv.tm_year + 1900);
  out.month = static_cast<uint8_t>(tmv.tm_mon + 1);
  out.day = static_cast<uint8_t>(tmv.tm_mday);
  out.hour = static_cast<uint8_t>(tmv.tm_hour);
  out.minute = static_cast<uint8_t>(tmv.tm_min);
  out.second = static_cast<uint8_t>(tmv.tm_sec);
  return true;
}

void timeBegin(const char*, const char*) {}
void yieldNow() {}
void restart() {}

void fillRandom(uint8_t* buf, size_t len) {
  static std::mt19937 rng(42);
  for (size_t i = 0; i < len; i++)
    buf[i] = static_cast<uint8_t>(rng());
}

HeapInfo heap() {
  HeapInfo h;
  h.free = 30000;
  h.maxBlock = 20000;
  h.fragmentation = 10;
  return h;
}

void macAddress(uint8_t mac[6]) {
  const uint8_t m[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  memcpy(mac, m, 6);
}

void macString(char out[18]) { snprintf(out, 18, "AA:BB:CC:DD:EE:FF"); }
ResetReason resetReason() { return native::g_reason; }
uint32_t freeSketchSpace() { return 1500000; }
void consoleWrite(const char* d, size_t n) { native::console().append(d, n); }
void consoleBegin(uint32_t) {}
size_t consoleRead(uint8_t* buf, size_t cap) {
  std::string& in = native::consoleInput();
  const size_t n = in.size() < cap ? in.size() : cap;
  memcpy(buf, in.data(), n);
  in.erase(0, n);
  return n;
}
void* tempAlloc(size_t n) { return malloc(n); }
void tempFree(void* p) { free(p); }

bool rtcRead(void* d, size_t n) {
  if (!native::g_rtcValid || n > 64) {
    memset(d, 0, n);
    return n <= 64;
  }
  memcpy(d, native::g_rtc, n);
  return true;
}
bool rtcWrite(const void* d, size_t n) {
  if (n > 64)
    return false;
  memcpy(native::g_rtc, d, n);
  native::g_rtcValid = true;
  return true;
}

bool aes256gcmSeal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t* aad,
                   size_t aadLen, uint8_t* buf, size_t len, uint8_t tag[16]) {
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  int outl = 0;
  bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) == 1 &&
            EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) == 1 &&
            EVP_EncryptUpdate(ctx, nullptr, &outl, aad, static_cast<int>(aadLen)) == 1 &&
            EVP_EncryptUpdate(ctx, buf, &outl, buf, static_cast<int>(len)) == 1 &&
            EVP_EncryptFinal_ex(ctx, buf + outl, &outl) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1;
  EVP_CIPHER_CTX_free(ctx);
  return ok;
}

} // namespace hal
} // namespace ps

#endif
