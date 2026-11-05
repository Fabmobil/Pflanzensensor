/**
 * @file errors.cpp
 * @brief Klartexte zu den Fehlercodes
 */
#include "core/types.h"

namespace ps {

namespace {
struct ErrEntry {
  char code[12];
  char text[40];
};

// Reihenfolge entspricht enum class Err.
const ErrEntry kErrors[] PS_PROGMEM = {
    {"ok", "OK"},
    {"unknown", "Unbekannter Fehler"},
    {"invalid", "Ungültiger Wert"},
    {"range", "Wert außerhalb des erlaubten Bereichs"},
    {"too_long", "Text zu lang"},
    {"not_found", "Nicht gefunden"},
    {"busy", "Gerade beschäftigt"},
    {"not_ready", "Noch nicht bereit"},
    {"disabled", "Deaktiviert"},
    {"no_space", "Kein Platz mehr"},
    {"io", "Lese- oder Schreibfehler"},
    {"corrupt", "Daten beschädigt"},
    {"timeout", "Zeitüberschreitung"},
    {"unsupported", "Nicht unterstützt"},
    {"auth", "Anmeldung erforderlich"},
    {"throttled", "Bitte kurz warten"},
    {"network", "Netzwerkfehler"},
    {"no_clock", "Uhrzeit noch nicht bekannt"},
    {"config", "Konfiguration unvollständig"},
    {"crypto", "Verschlüsselung fehlgeschlagen"},
    {"hardware", "Hardwarefehler"},
    {"parse", "Format nicht lesbar"},
};
constexpr size_t kErrCount = sizeof(kErrors) / sizeof(kErrors[0]);
static_assert(kErrCount == static_cast<size_t>(Err::ParseError) + 1,
              "Fehlertabelle passt nicht zu enum class Err");
} // namespace

FStr errCode(Err e) {
  size_t i = static_cast<size_t>(e);
  if (i >= kErrCount)
    i = static_cast<size_t>(Err::Unknown);
  return FStr{kErrors[i].code};
}

FStr errText(Err e) {
  size_t i = static_cast<size_t>(e);
  if (i >= kErrCount)
    i = static_cast<size_t>(Err::Unknown);
  return FStr{kErrors[i].text};
}

} // namespace ps
