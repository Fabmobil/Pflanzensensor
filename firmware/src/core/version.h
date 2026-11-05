/**
 * @file version.h
 * @brief Firmwareversion (aus der Datei VERSION, gesetzt von tools/embed_assets.py)
 */
#pragma once

// Baudatum aus tools/embed_assets.py; __DATE__ wäre in Nix-Shells "Jan  1 1980"
#if __has_include("core/build_date.h")
#include "core/build_date.h"
#endif
#ifndef PS_BUILD_DATE
#define PS_BUILD_DATE __DATE__
#endif

#ifndef PS_VERSION
#define PS_VERSION "0.0.0-dev"
#endif

namespace ps {
constexpr const char* kVersion = PS_VERSION;
constexpr const char* kBuildDate = PS_BUILD_DATE;
} // namespace ps
