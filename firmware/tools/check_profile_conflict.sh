#!/usr/bin/env bash
# Abnahme AP-0: Ein absichtlich doppelt belegter Pin muss den Build brechen.
# Übersetzt board.h mit einem Testprofil, in dem DHT und LED denselben Pin
# haben, und erwartet einen Compilerfehler mit der Meldung aus profile_check.h.
set -uo pipefail
cd "$(dirname "$0")/.."
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
sed 's/p.dht = 4;/p.dht = 5;/' boards/native_test.h > "$TMP/conflict.h"
echo '#include "board.h"' > "$TMP/t.cpp"
OUT=$(g++ -std=gnu++17 -fsyntax-only -I "$TMP" -I boards -I src -DBOARD_PROFILE='"conflict.h"' "$TMP/t.cpp" 2>&1)
if echo "$OUT" | grep -q "ein Pin ist doppelt belegt"; then
  echo "check_profile_conflict: doppelter Pin wird erkannt"
else
  echo "check_profile_conflict: FEHLER - doppelter Pin wurde nicht erkannt"
  echo "$OUT" | head -20
  exit 1
fi
# Gegenprobe: das Testprofil selbst ist gültig
g++ -std=gnu++17 -fsyntax-only -I boards -I src -DBOARD_PROFILE='"native_test.h"' "$TMP/t.cpp" || exit 1
