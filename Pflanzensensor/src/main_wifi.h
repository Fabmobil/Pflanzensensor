/**
 * @file main_wifi.h
 * @brief WiFi and NTP initialization module header
 */

#ifndef MAIN_WIFI_H
#define MAIN_WIFI_H

#include "utils/result_types.h"
#include <Arduino.h>

ResourceResult setupWiFiWithDisplay(bool showDisplay = false);

#endif // MAIN_WIFI_H
