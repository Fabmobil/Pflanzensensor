/**
 * @file native.h
 * @brief Fakes der Plattform für die nativen Tests (steuerbare Uhr, RAM-Dateisystem)
 */
#pragma once

#if !defined(ARDUINO)

#include <map>
#include <string>
#include <vector>

#include "hal/fs.h"
#include "hal/system.h"
#include "hal/tcp_client.h"
#include "hal/wifi.h"

namespace ps {
namespace native {

/// Uhr der Tests
void setMillis(uint32_t ms);
void advance(uint32_t ms);
void setEpoch(uint32_t epoch);
void setResetReason(hal::ResetReason r);
/// Konsolenausgabe einsammeln
std::string& console();
/// Was die Konsole als Nächstes "empfängt" (Improv-Tests)
std::string& consoleInput();

/// Dateisystem im RAM
class MemFs final : public hal::Fs {
public:
  bool mount() override { return true; }
  bool format() override {
    files_.clear();
    return true;
  }
  hal::File* open(const char* path, hal::OpenMode mode) override;
  void close(hal::File* f) override;
  bool exists(const char* path) override { return files_.count(path) > 0; }
  bool remove(const char* path) override { return files_.erase(path) > 0; }
  bool rename(const char* from, const char* to) override;
  bool info(hal::FsInfo& out) override;
  void list(hal::ListCallback cb, void* ctx) override;
  /// Verzeichnisse ergeben sich aus "/" im Dateinamen
  void listRoot(hal::EntryCallback cb, void* ctx) override;
  bool removeTree(const char* path) override;

  uint32_t total = 950 * 1024;
  std::map<std::string, std::vector<uint8_t>> files_;

  class Handle : public hal::File {
  public:
    size_t read(uint8_t* buf, size_t len) override;
    size_t writeBytes(const uint8_t* buf, size_t len) override;
    bool seek(uint32_t pos) override;
    uint32_t position() override { return pos; }
    uint32_t size() override { return static_cast<uint32_t>(data->size()); }
    bool truncate(uint32_t len) override;
    std::vector<uint8_t>* data = nullptr;
    MemFs* fs = nullptr;
    uint32_t pos = 0;
    bool used = false;
  };

private:
  Handle slots_[6];
};

/// WLAN-Fake
class FakeWifi final : public hal::Wifi {
public:
  void setHostname(const char* h) override { host = h; }
  void beginSta(const char* s, const char* p) override {
    ssidTried.push_back(s);
    staSsid = s;
    status = hal::StaStatus::Connecting;
    if (reachable.count(s))
      status = hal::StaStatus::Connected;
  }
  hal::StaStatus staStatus() override { return status; }
  void disconnectSta() override { status = hal::StaStatus::Idle; }
  bool startAp(const char* s, bool) override {
    ap = true;
    apName = s;
    return true;
  }
  void stopAp() override { ap = false; }
  void staIp(char* o, size_t c) override {
    snprintf(o, c, "%s", status == hal::StaStatus::Connected ? "192.168.1.23" : "0.0.0.0");
  }
  void apIp(char* o, size_t c) override { snprintf(o, c, "192.168.4.1"); }
  void ssid(char* o, size_t c) override { snprintf(o, c, "%s", staSsid.c_str()); }
  int8_t rssi() override { return -60; }
  void dnsStart() override { dns = true; }
  void dnsLoop() override {}
  void dnsStop() override { dns = false; }
  bool mdnsBegin(const char* h) override {
    mdnsHost = h;
    return true;
  }
  void mdnsLoop() override {}
  void mdnsEnd() override {}

  std::string host, staSsid, apName, mdnsHost;
  std::vector<std::string> ssidTried;
  std::map<std::string, bool> reachable;
  hal::StaStatus status = hal::StaStatus::Idle;
  bool ap = false, dns = false;
};

/// TCP-Fake: sammelt das Gesendete, liefert eine vorgegebene Antwort
class FakeTcp final : public hal::TcpClient {
public:
  bool connect(const char* h, uint16_t p, uint32_t) override {
    host = h;
    port = p;
    sent.clear();
    readPos = 0;
    open = connectOk;
    return connectOk;
  }
  size_t writeBytes(const uint8_t* d, size_t n) override {
    sent.append(reinterpret_cast<const char*>(d), n);
    return n;
  }
  int available() override { return open ? static_cast<int>(response.size() - readPos) : 0; }
  int read(uint8_t* b, size_t n) override {
    size_t k = response.size() - readPos;
    if (k > n)
      k = n;
    memcpy(b, response.data() + readPos, k);
    readPos += k;
    return static_cast<int>(k);
  }
  bool connected() override { return open && readPos < response.size(); }
  void stop() override { open = false; }

  bool connectOk = true;
  bool open = false;
  std::string host, sent, response = "HTTP/1.1 200 OK\r\n\r\n{\"ok\":true}";
  uint16_t port = 0;
  size_t readPos = 0;
};

} // namespace native
} // namespace ps

#endif
