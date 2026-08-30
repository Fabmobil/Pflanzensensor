#!/usr/bin/env bash
#
# Führt die JavaScript-Tests aus test/js/ aus.
#
# Warum eine eigene Datei und nicht "pio test": PlatformIO baut C++ gegen Unity
# und weiß mit .mjs nichts anzufangen. test/js/ liegt trotzdem unter test/,
# damit alle Tests an einer Stelle stehen - PlatformIO überspringt das
# Verzeichnis, weil dort keine übersetzbaren Quellen liegen (geprüft: die 49
# nativen Tests laufen unverändert weiter).
#
# Getestet wird data/js/devicewait.js, also die Datei, die auch ausgeliefert
# wird - sie wird über test/js/helpers/load.mjs unverändert in eine Sandbox
# geladen. Keine Abhängigkeiten: node bringt Testläufer (node:test) und
# Zusicherungen (node:assert) selbst mit, es gibt kein package.json und kein
# npm install.
#
# Verwendung: ./run_js_tests.sh
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v node >/dev/null 2>&1; then
  echo "node nicht gefunden - im Entwicklungs-Shell (nix develop) ist es enthalten." >&2
  exit 1
fi

echo "Führe JavaScript-Tests aus (node $(node --version))..."
node --test test/js/*.test.mjs
