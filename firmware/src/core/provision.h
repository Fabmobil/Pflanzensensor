/**
 * @file provision.h
 * @brief Provisionierungsblock des Webflashers (4-KB-Sektor neben der Firmware)
 * @details Aufbau: "PSPROV1\0" | Länge (u32 LE) | CRC32 (u32 LE) | JSON | 0xFF…
 *          {
 *            "hardware": {"name": "pcb_v1", "features": {…}, "pins": {…},
 *                         "staticIp": {"ip": "192.168.1.50", …}},
 *            "settings": {"deviceName": "…", "adminPassword": "…",
 *                         "wifi": [{"ssid": "…", "password": "…"}, null, {…}],
 *                         "channelNames": {"ANALOG_0": "Licht", …},
 *                         "config": {…}}
 *          }
 *          "wifi" hat je Slot einen Eintrag; null lässt den Slot unverändert.
 *          "channelNames" benennt Messkanäle (Schlüssel wie in der Oberfläche).
 *          "config" ist eine Konfigurationsdatei (config.json der Firmware 2.x
 *          oder Export mit "schema": 1), die der Webflasher importiert hat; das
 *          Gerät übernimmt sie mit ConfigService::importJson() vor den übrigen
 *          Einstellungen (die also Vorrang haben). parse() merkt sich nur, dass
 *          sie da ist (hasConfig); gelesen wird sie mit readConfig().
 *          Die Hardware bleibt im Block und gilt bei jedem Start (auch nach
 *          OTA-Updates). Die Einstellungen übernimmt das Gerät einmal; danach
 *          schreibt es den Block ohne sie zurück (keine Passwörter im Klartext
 *          im Flash, spätere Änderungen in der Oberfläche bleiben).
 *          Gegenstück im Browser: webflasher/provision.js.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <ArduinoJson.h>

#include "core/board_types.h"
#include "core/types.h"

namespace ps {
namespace provision {

constexpr size_t kSectorSize = 4096;
constexpr size_t kHeaderSize = 16;
constexpr char kMagic[8] = {'P', 'S', 'P', 'R', 'O', 'V', '1', '\0'};

constexpr uint8_t kWifiSlots = 3;
/// Höchstens so viele Kanalnamen (größte Kanalzahl, ESP32)
constexpr uint8_t kMaxChannelNames = 31;

struct WifiSetting {
  bool set = false;
  char ssid[33] = {};
  char password[65] = {};
};

struct ChannelName {
  char key[20] = {};  ///< "ANALOG_0"
  char name[25] = {}; ///< wie ChannelConfig::name (24 Byte UTF-8)
};

struct Settings {
  bool hasDeviceName = false;
  char deviceName[33] = {};
  WifiSetting wifi[kWifiSlots];
  bool hasAdminPassword = false;
  char adminPassword[33] = {};
  uint8_t channelNameCount = 0;
  ChannelName channelNames[kMaxChannelNames];
  /// Block enthält "settings.config" (Inhalt über readConfig())
  bool hasConfig = false;
  bool anyWifi() const {
    for (const auto& w : wifi)
      if (w.set)
        return true;
    return false;
  }
  bool any() const {
    return hasDeviceName || anyWifi() || hasAdminPassword || channelNameCount > 0 || hasConfig;
  }
};

struct Block {
  bool hasHardware = false;
  Hardware hardware;
  Settings settings;
};

enum class Result : uint8_t { Ok, Empty, BadMagic, BadLength, BadCrc, BadJson };

/// Sektor lesen. Empty: unbeschrieben (0xFF) - das ist der Normalfall ohne Webflasher.
/// Hardware-Felder, die fehlen, kommen aus base (Voreinstellung).
Result parse(const uint8_t* sector, size_t len, const Hardware& base, Block& out);

/**
 * @brief Nur "settings.config" lesen (Filter, damit der Rest keinen Heap braucht)
 * @details Ok: doc["settings"]["config"] ist das Objekt. Prüft Kopf und CRC wie parse().
 */
Result readConfig(const uint8_t* sector, size_t len, JsonDocument& doc);

/// Block schreiben (Header + JSON + 0xFF-Auffüllung auf kSectorSize).
/// withSettings=false lässt die Einstellungen weg; "config" wird nie
/// geschrieben (das Gerät erzeugt sie nicht). Rückgabe: false bei Überlänge.
bool encode(const Block& b, bool withSettings, uint8_t* sector, size_t len);

const char* resultName(Result r);

} // namespace provision
} // namespace ps
