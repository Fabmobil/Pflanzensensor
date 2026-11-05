/**
 * @file profile_check.h
 * @brief Compile-Zeit-Prüfung der Voreinstellung eines Board-Profils
 * @details Dieselben Regeln wie zur Laufzeit (core/hardware_check.h). Bricht
 *          den Build ab bei doppelt belegten Pins, Flash-/Strapping-Pins und
 *          unmöglichen Kombinationen.
 */
#pragma once

#include "core/hardware_check.h"

namespace board_profile {
namespace check {
constexpr ps::HwProblem kProblem = ps::checkHardware(kPlatform, kFeatures, kPins).problem;
static_assert(kProblem != ps::HwProblem::DuplicatePin, "Board-Profil: ein Pin ist doppelt belegt");
static_assert(kProblem != ps::HwProblem::FlashPin && kProblem != ps::HwProblem::StrapPin &&
                  kProblem != ps::HwProblem::InputOnlyPin,
              "Board-Profil: Strapping- oder Flash-Pin unzulässig genutzt");
static_assert(kProblem != ps::HwProblem::MissingPin, "Board-Profil: bestückte Peripherie ohne Pin");
static_assert(kProblem != ps::HwProblem::NoSuchPin && kProblem != ps::HwProblem::AdcPin,
              "Board-Profil: Pin gibt es nicht oder kann nicht analog messen");
static_assert(kProblem != ps::HwProblem::AnalogChannels,
              "Board-Profil: analogChannels muss 1..8 sein");
static_assert(kProblem != ps::HwProblem::NeedMultiplexer,
              "Board-Profil: mehr als ein Analogkanal braucht den Multiplexer");
static_assert(kProblem != ps::HwProblem::Ds18b20Count, "Board-Profil: ds18b20Max muss 1..8 sein");
static_assert(kProblem != ps::HwProblem::TooManyChannels, "Board-Profil: zu viele Kanäle");
static_assert(kProblem != ps::HwProblem::NoSensor, "Board-Profil: kein Sensor bestückt");
static_assert(kProblem != ps::HwProblem::Mhz19Range,
              "Board-Profil: mhz19Range muss 2000 oder 5000 sein");
} // namespace check
} // namespace board_profile
