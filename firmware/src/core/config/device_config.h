/**
 * @file device_config.h
 * @brief Gerätekonfiguration und Geheimnisse als POD
 * @details Feste Größen (refactor.md 5.3, Budget ≤ 1 KB). Geheimnisse liegen
 *          getrennt und werden nie exportiert oder an den Browser gegeben.
 */
#pragma once

#include "core/types.h"

namespace ps {

using ChannelKey = FixedString<24>; ///< "ANALOG_1", "SERIAL_RECEIVER_6"

enum class LogLevel : uint8_t { Debug = 0, Info, Warning, Error };

struct DeviceConfig {
  // general
  FixedString<33> deviceName;
  bool otaRequireMd5 = false;
  ChannelKey flowerChannel;
  // wifi (Passwörter in Secrets)
  FixedString<33> wifiSsid[3];
  // display
  uint8_t screenDurationS = 5;
  uint8_t clockFormat = 0; ///< 0 = 24h, 1 = 12h (AM/PM)
  bool showIp = true;
  bool showClock = true;
  bool showQr = false;
  bool showFlower = true;
  bool showFabmobil = true;
  // led
  uint8_t ledMode = 2; ///< 0 aus, 1 folgt Display, 2 fester Kanal
  ChannelKey ledChannel;
  bool ledOnlyRed = false;
  // log
  uint8_t logLevel = static_cast<uint8_t>(LogLevel::Info);
  bool fileLog = false;
  bool debugRam = false;
  bool debugCycle = false;
  bool debugSensor = false;
  bool debugDisplay = false;
  bool debugWeb = false;
  bool debugMail = false;
  // mail
  bool mailEnabled = false;
  FixedString<97> mailUrl;
  FixedString<33> mailDeviceId;
  FixedString<97> mailTo;
  FixedString<200> mailWatch; ///< kommagetrennte Kanalschlüssel, leer = alle
  uint8_t mailWarnFrom = 0;   ///< 0 = gelb, 1 = rot
  uint16_t mailWarnHours = 4;
  bool mailBoot = false;
  bool mailAlive = false;
  uint16_t mailAliveHours = 24;
};

struct Secrets {
  FixedString<65> wifiPassword[3];
  bool adminSet = false; ///< false: Build-Standardpasswort gilt
  uint8_t adminSalt[16] = {};
  uint8_t adminHash[32] = {};
  FixedString<48> mailKey; ///< Base64 von 32 Byte
};

/// Bereiche für ConfigChanged-Ereignisse (Bitmaske)
enum ConfigSection : uint8_t {
  SecGeneral = 1,
  SecWifi = 2,
  SecDisplay = 4,
  SecLed = 8,
  SecLog = 16,
  SecMail = 32,
};

} // namespace ps
