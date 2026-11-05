#!/usr/bin/env bash
# Testabdeckung der nativen Tests (gcov/gcovr). Gleiche Variablen wie
# run_native_tests.sh; Bericht nach .pio/coverage/index.html.
#   UNITY=… ARDUINOJSON=… QRCODE=… tools/coverage.sh
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=.pio/native-cov
rm -rf "$OUT"
OUT="$OUT" EXTRA_FLAGS="--coverage" tools/run_native_tests.sh "$@" >/dev/null || { echo "Tests fehlgeschlagen"; exit 1; }
mkdir -p .pio/coverage
GCOVR=${GCOVR:-gcovr}
"$GCOVR" -r . --object-directory "$OUT" --filter 'src/' --exclude 'src/web/assets.h' \
  --html-details .pio/coverage/index.html --txt - | tail -60
