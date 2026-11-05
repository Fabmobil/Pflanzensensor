#!/usr/bin/env bash
# Prüft, dass in core/, services/ und web/ keine Heap-Konstrukte stehen
# (refactor.md 5.1 Punkt 3): String, std::string, std::vector, std::function,
# std::map und new (Placement-new ist erlaubt).
set -euo pipefail
cd "$(dirname "$0")/.."
DIRS=(src/core src/services src/web)
PATTERN='(\bString\b|std::string\b|std::vector|std::function|std::map|\bnew\s+[A-Za-z_])'
HITS=$(grep -rnE "$PATTERN" "${DIRS[@]}" --include='*.h' --include='*.cpp' |
  grep -vE '^\S+:\s*(//|\*|/\*)' | grep -v 'string_view' | grep -v 'assets.h' || true)
if [ -n "$HITS" ]; then
  echo "Verbotene Konstrukte gefunden:"
  echo "$HITS"
  exit 1
fi
echo "check_forbidden: in Ordnung"
