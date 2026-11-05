#!/usr/bin/env bash
# JavaScript-Tests der Weboberfläche (node --test, keine Abhängigkeiten)
set -euo pipefail
cd "$(dirname "$0")/.."
node --test test/js/*.test.mjs
