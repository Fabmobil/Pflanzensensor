/**
 * @file provision.h
 * @brief Provisionierungsblock des Webflashers (4-KB-Sektor neben der Firmware)
 * @details Aufbau: "PSPROV1\0" | Länge (u32 LE) | CRC32 (u32 LE) | JSON | 0xFF…
 *          {
 *            "hardware": {"name": "pcb_v1", "features": {…}, "pins": {…},
 *                         "staticIp": {"ip": "192.168.1.50", …}},
 *            "settings": {"deviceName": "…", "wifiSsid": "…",
 *                         "wifiPassword": "…", "adminPassword": "…"}
 *          }
 *          Die Hardware bleibt im Block und gilt bei jedem Start (auch nach
 *          OTA-Updates). Die Einstellungen übernimmt das Gerät einmal; danach
 *          schreibt es den Block ohne sie zurück (keine Passwörter im Klartext
 *          im Flash, spätere Änderungen in der Oberfläche bleiben).
 *          Gegenstück im Browser: webflasher/provision.js.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/board_types.h"
#include "core/types.h"

namespace ps {
namespace provision {

constexpr size_t kSectorSize = 4096;
constexpr size_t kHeaderSize = 16;
constexpr char kMagic[8] = {'P', 'S', 'P', 'R', 'O', 'V', '1', '\0'};

struct Settings {
  bool hasDeviceName = false;
  char deviceName[33] = {};
  bool hasWifi = false;
  char wifiSsid[33] = {};
  char wifiPassword[65] = {};
  bool hasAdminPassword = false;
  char adminPassword[33] = {};
  bool any() const { return hasDeviceName || hasWifi || hasAdminPassword; }
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

/// Block schreiben (Header + JSON + 0xFF-Auffüllung auf kSectorSize).
/// withSettings=false lässt die Einstellungen weg. Rückgabe: false bei Überlänge.
bool encode(const Block& b, bool withSettings, uint8_t* sector, size_t len);

const char* resultName(Result r);

} // namespace provision
} // namespace ps
