#!/usr/bin/env bash
#
# Baut die Firmware neu und meldet Compilerwarnungen aus dem EIGENEN Quelltext.
#
# Warum gefiltert wird: -Wall/-Wextra/-Wshadow gelten über build_src_flags nur
# für Pflanzensensor/src/. Trotzdem erzeugt ein sauberer Neubau rund 1380
# Warnungen - sie entstehen *in* Framework- und Bibliotheksheadern, die unsere
# Quellen einbinden, und werden beim Übersetzen unserer Übersetzungseinheit
# gemeldet. Allein StreamString.h des Frameworks steuert 1170 bei.
#
# Diese Header lassen sich nicht sinnvoll stummschalten: der -isystem-Weg
# scheitert daran, dass PlatformIO die Framework-Includes pro Ziel setzt und
# nicht über die globale CPPPATH, an die ein extra_script herankommt.
#
# Also wird stattdessen gefiltert. Damit ist eine neue Warnung im eigenen Code
# sofort sichtbar, statt in fremdem Rauschen unterzugehen - und die Prüfung
# taugt unverändert für die CI.
#
# Aufruf:  ./check_warnings.sh [-q]
#   -q   nur die Zusammenfassung ausgeben
# Rückgabewert: 0 = keine eigenen Warnungen, 1 = Warnungen gefunden, 2 = Baufehler

set -uo pipefail

QUIET=0
[ "${1:-}" = "-q" ] && QUIET=1

cd "$(dirname "$0")" || exit 2

ENV_NAME=Pflanzensensor
LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

echo "Baue $ENV_NAME neu (nur eigene Quellen)..."
find ".pio/build/$ENV_NAME/src" -name '*.o' -delete 2>/dev/null

if ! pio run -e "$ENV_NAME" > "$LOG" 2>&1; then
	echo "FEHLER: Bau fehlgeschlagen."
	grep -E 'error:|Error' "$LOG" | head -20
	exit 2
fi

# Nur Warnungen, deren Fundstelle in unserem Quelltext liegt
OWN=$(grep -P '^Pflanzensensor/src/\S+:\d+:\d+: warning:' "$LOG" | sort -u)
FOREIGN_COUNT=$(( $(grep -c 'warning:' "$LOG") - $(printf '%s' "$OWN" | grep -c . ) ))
OWN_COUNT=$(printf '%s' "$OWN" | grep -c .)

if [ "$QUIET" -eq 0 ] && [ "$OWN_COUNT" -gt 0 ]; then
	echo
	echo "$OWN"
	echo
fi

echo "Eigener Quelltext: $OWN_COUNT Warnung(en)"
echo "Fremde Header:     $FOREIGN_COUNT (Framework/Bibliotheken, nicht behebbar)"

grep -E '^(RAM|Flash):' "$LOG"

[ "$OWN_COUNT" -eq 0 ] || exit 1
