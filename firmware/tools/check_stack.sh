#!/usr/bin/env bash
# Baut die Firmware mit -fstack-usage und meldet Funktionen im EIGENEN Code,
# deren Stapelrahmen den Grenzwert überschreitet (refactor.md 5.3: ≤ 512 B).
# Der ESP8266 gibt dem Loop-Task nur 4 KB Stapel.
#
# Aufruf:  tools/check_stack.sh [-q] [Grenzwert] [env]
# Umgebung: PIO (Pfad zu pio), PIO_CONF (z.B. platformio.local.ini)
# Rückgabe: 0 = in Ordnung, 1 = zu große Rahmen, 2 = Baufehler
set -uo pipefail
QUIET=0
[ "${1:-}" = "-q" ] && { QUIET=1; shift; }
LIMIT="${1:-512}"
ENV_NAME="${2:-esp8266_pcb}"
PIO="${PIO:-pio}"
CONF=()
[ -n "${PIO_CONF:-}" ] && CONF=(-c "$PIO_CONF")
cd "$(dirname "$0")/.." || exit 2
BUILD_DIR=".pio/build/$ENV_NAME"
[ "$QUIET" = 1 ] || echo "Baue $ENV_NAME mit -fstack-usage ..."
if ! PLATFORMIO_BUILD_SRC_FLAGS="-fstack-usage" "$PIO" run "${CONF[@]}" -e "$ENV_NAME" >/dev/null 2>&1; then
  echo "Baufehler - siehe '$PIO run -e $ENV_NAME'" >&2
  exit 2
fi
FOUND=$(find "$BUILD_DIR/src" -name '*.su' -print0 2>/dev/null | xargs -0 cat 2>/dev/null |
  awk -F'\t' '{ split($1, o, ":"); print $2, o[1] ":" o[2], $3 }' | sort -rn)
if [ -z "$FOUND" ]; then
  echo "Keine .su-Dateien gefunden" >&2
  exit 2
fi
[ "$QUIET" = 1 ] || { echo "Größte Rahmen:"; echo "$FOUND" | head -10; }
OVER=$(echo "$FOUND" | awk -v g="$LIMIT" '$1 > g')
if [ -n "$OVER" ]; then
  echo "Stapelrahmen über $LIMIT B:"
  echo "$OVER"
  exit 1
fi
echo "Alle Stapelrahmen ≤ $LIMIT B"
