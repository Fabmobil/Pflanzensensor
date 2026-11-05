/**
 * @file defaults.h
 * @brief Build-Standardwerte der Gerätekonfiguration
 * @details Zugangsdaten stehen hier nicht. Für Entwickler darf eine nicht
 *          versionierte boards/secrets.h WLAN-Daten vorbelegen
 *          (-D PS_DEV_SECRETS); der Release-Build lehnt das ab.
 */
#pragma once

#include "core/config/device_config.h"
#include "core/config/sensor_config.h"

namespace ps {
namespace defaults {

constexpr const char* kDeviceName = "Fabmobil Pflanzensensor";
/// Initiales Admin-Passwort, solange keines gesetzt wurde.
constexpr const char* kAdminPassword = "Fabmobil";
constexpr const char* kMailUrl = "http://pflanzensensormailbot.fabmobil.org:45480";
constexpr const char* kFlowerChannel = "ANALOG_1";
constexpr const char* kLedChannel = "ANALOG_1";

/// Gerätekonfiguration mit Standardwerten
void device(DeviceConfig& c);
void secrets(Secrets& s);

} // namespace defaults
} // namespace ps
