#!/usr/bin/env bash
# Native Tests ohne PlatformIO (g++ + Unity), z.B. offline oder für schnelle
# Läufe. Der reguläre Weg ist "pio test -e native".
#   UNITY=/pfad/Unity ARDUINOJSON=/pfad/ArduinoJson/src QRCODE=/pfad/QRCode/src tools/run_native_tests.sh [test_name] [--vectors]
set -euo pipefail
cd "$(dirname "$0")/.."
: "${UNITY:?UNITY (Quellverzeichnis von ThrowTheSwitch/Unity) setzen}"
: "${ARDUINOJSON:?ARDUINOJSON (ArduinoJson/src) setzen}"
: "${QRCODE:?QRCODE (ricmoo/QRCode/src) setzen}"
OUT=${OUT:-.pio/native-g++}
mkdir -p "$OUT"
python3 tools/embed_assets.py >/dev/null
FLAGS=(-std=gnu++17 -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
       -I src -I boards -I test/native/support -I "$UNITY/src" -I "$ARDUINOJSON" -I "$QRCODE"
       -DBOARD_PROFILE='"native_test.h"' -DNATIVE_BUILD -DUNITY_INCLUDE_DOUBLE ${EXTRA_FLAGS:-})
SRCS="$(find src/core src/services src/web src/platform/native -name '*.cpp') src/app/boot_guard.cpp src/app/scheduler.cpp"
# Bibliothek einmal bauen
LIB="$OUT/libps.a"
if [ ! -f "$LIB" ] || [ -n "$(find src -newer "$LIB" -name '*.[ch]*' | head -1)" ]; then
  rm -f "$OUT"/*.o
  for f in $SRCS "$QRCODE/qrcode.c"; do
    o="$OUT/$(echo "$f" | tr '/' '_').o"
    if [[ $f == *.c ]]; then gcc ${EXTRA_FLAGS:-} -c -I "$QRCODE" "$f" -o "$o"; else g++ "${FLAGS[@]}" -c "$f" -o "$o"; fi
  done
  ar rcs "$LIB" "$OUT"/*.o
fi
if [[ " $* " == *" --vectors "* ]]; then
  g++ "${FLAGS[@]}" tools/gen_vectors.cpp "$LIB" -lcrypto -o "$OUT/gen_vectors"
  "$OUT/gen_vectors" test/vectors
fi
fail=0
for d in test/native/test_*; do
  name=$(basename "$d")
  if [ $# -gt 0 ] && [[ $1 != --* ]] && [ "$1" != "$name" ]; then continue; fi
  g++ "${FLAGS[@]}" "$d"/*.cpp "$UNITY/src/unity.c" "$LIB" ${EXTRA_FLAGS:-} -lcrypto -o "$OUT/$name"
  echo "== $name"
  "$OUT/$name" || fail=1
done
exit $fail
