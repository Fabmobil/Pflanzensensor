/**
 * @file display_service.cpp
 */
#include "services/display_service.h"

#include <stdio.h>

#include "core/display/screens.h"
#include "hal/system.h"
#include "services/log.h"

namespace ps {

namespace {
constexpr uint8_t kScreenCount = 5 + board::kMaxSlots; // Bildschirme ohne Kanal werden übersprungen
}

void DisplayService::begin() {
  if (!board::features().display)
    return;
  present_ = bus_.begin(board::features().displayAddress);
  if (!present_) {
    LOGW(Mod::Display, "Kein Display gefunden - es geht ohne weiter");
    return;
  }
  bootHeader(false);
}

void DisplayService::bootHeader(bool update) {
  snprintf(header_, sizeof(header_), "%s %s:", cfg_.device().deviceName.c_str(),
           update ? "Update" : "startet");
  lineCount_ = 0;
  boot_ = true;
  renderBoot();
}

void DisplayService::bootLine(const char* line) {
  if (lineCount_ == 6) {
    for (uint8_t i = 1; i < 6; i++)
      memcpy(lines_[i - 1], lines_[i], sizeof(lines_[0]));
    lineCount_ = 5;
  }
  strncpy(lines_[lineCount_], line, sizeof(lines_[0]) - 1);
  lines_[lineCount_][sizeof(lines_[0]) - 1] = '\0';
  lineCount_++;
  if (boot_)
    renderBoot();
}

void DisplayService::renderBoot() {
  if (!present_)
    return;
  const char* ptrs[6];
  for (uint8_t i = 0; i < lineCount_; i++)
    ptrs[i] = lines_[i];
  screens::bootLog(canvas_, header_, ptrs, lineCount_);
  bus_.flush(canvas_.data());
  pushPage_ = hal::DisplayBus::kPages;
}

void DisplayService::endBoot() {
  boot_ = false;
  forceNext_ = true;
}

void DisplayService::otaProgress(uint8_t percent) {
  if (!present_)
    return;
  ota_ = true;
  screens::progress(canvas_, "Firmware-Update", percent);
  bus_.flush(canvas_.data());
  pushPage_ = hal::DisplayBus::kPages;
}

bool DisplayService::screenAvailable(uint8_t s) const {
  const DeviceConfig& d = cfg_.device();
  switch (s) {
  case Info:
    return d.showIp;
  case Clock:
    return d.showClock && epochValid(hal::epoch());
  case Qr:
    return d.showQr;
  case Flower:
    return d.showFlower;
  case Fabmobil:
    return d.showFabmobil;
  default: {
    const uint8_t slot = static_cast<uint8_t>(s - FirstChannel);
    if (slot >= board::slotCount())
      return false;
    const ChannelConfig& cc = cfg_.sensors().channels[slot];
    const uint8_t sensor = board::slot(slot).sensor;
    return cc.enabled && cc.display && cfg_.sensors().sensors[sensor].enabled;
  }
  }
}

void DisplayService::render(uint8_t s) {
  char ip[16], ssid[33];
  net_.ip(ip, sizeof(ip));
  net_.ssid(ssid, sizeof(ssid));
  currentSlot_ = -1;
  switch (s) {
  case Info:
    screens::info(canvas_, net_.hostname(), ip, ssid);
    LOGD(Mod::Display, "Zeigt Netzwerk: %s.local, %s, WLAN %s", net_.hostname(), ip, ssid);
    break;
  case Clock: {
    hal::LocalTime lt{};
    hal::toLocalTime(hal::epoch(), lt);
    const char* ampm = nullptr;
    uint8_t h = lt.hour;
    if (cfg_.device().clockFormat == 1) {
      ampm = h < 12 ? "AM" : "PM";
      h = static_cast<uint8_t>(h % 12 == 0 ? 12 : h % 12);
    }
    screens::clock(canvas_, h, lt.minute, lt.day, lt.month, lt.year, ampm);
    LOGD(Mod::Display, "Zeigt Uhr: %02u:%02u%s%s, %02u.%02u.%04u", h, lt.minute, ampm ? " " : "",
         ampm ? ampm : "", lt.day, lt.month, lt.year);
    break;
  }
  case Qr: {
    char url[32];
    snprintf(url, sizeof(url), "http://%s", ip);
    screens::qr(canvas_, url);
    LOGD(Mod::Display, "Zeigt QR-Code: %s", url);
    break;
  }
  case Flower:
    screens::flower(canvas_);
    LOGD(Mod::Display, "Zeigt Blume");
    break;
  case Fabmobil:
    screens::fabmobil(canvas_);
    LOGD(Mod::Display, "Zeigt Fabmobil-Logo");
    break;
  default: {
    const uint8_t slot = static_cast<uint8_t>(s - FirstChannel);
    const ChannelConfig& cc = cfg_.sensors().channels[slot];
    const float v = sensors_.channel(slot).value;
    screens::channel(canvas_, cc.name.c_str(), v, cc.unit.c_str());
    if (isnan(v))
      LOGD(Mod::Display, "Zeigt %s: kein Wert", cc.name.c_str());
    else
      LOGD(Mod::Display, "Zeigt %s: %.1f %s", cc.name.c_str(), static_cast<double>(v),
           cc.unit.c_str());
    currentSlot_ = slot;
    break;
  }
  }
  startPush();
}

void DisplayService::tick(Instant now) {
  if (!present_ || boot_ || ota_)
    return;
  if (pushPage_ < hal::DisplayBus::kPages) {
    bus_.flushPages(canvas_.data(), pushPage_, kPagesPerTick);
    pushPage_ = static_cast<uint8_t>(pushPage_ + kPagesPerTick);
    return;
  }
  const int32_t dur = cfg_.device().screenDurationS * 1000;
  if (!forceNext_ && (now - lastSwitch_).millis() < dur)
    return;
  forceNext_ = false;
  lastSwitch_ = now;
  // Nächsten verfügbaren Bildschirm suchen; nicht verfügbare verbrauchen keinen Schritt
  const uint8_t base = screen_ == 0xFF ? static_cast<uint8_t>(kScreenCount - 1) : screen_;
  for (uint8_t i = 1; i <= kScreenCount; i++) {
    const uint8_t s = static_cast<uint8_t>((base + i) % kScreenCount);
    if (!screenAvailable(s))
      continue;
    screen_ = s;
    render(s);
    emptyLogged_ = false;
    return;
  }
  // Nichts aktiviert: leer
  if (!emptyLogged_)
    LOGI(Mod::Display, "Kein Bildschirm aktiviert - Display bleibt leer");
  emptyLogged_ = true;
  canvas_.clear();
  startPush();
  currentSlot_ = -1;
}

} // namespace ps
