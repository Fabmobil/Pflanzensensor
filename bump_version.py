#!/usr/bin/env python3
"""
Hebt die letzte Stelle der Version in configs/config.h an.

Gedacht als pre-commit-Hook: sind C++-Quellen unterhalb Pflanzensensor/src
vorgemerkt, bekommt der Commit eine neue Patch-Version.

Zwei Einschraenkungen mit Absicht:

  * Wurde die VERSION-Zeile in diesem Commit schon von Hand geaendert, passiert
    nichts. Ein bewusster Sprung auf 2.29.0 soll nicht zu 2.29.1 verrutschen.
  * Geaendert wird nur bei C++-Dateien unter Pflanzensensor/src. Aenderungen an
    Tests, Skripten, CI oder data/ zaehlen nicht als Firmware-Aenderung.

Zu bedenken: die Version steigt damit bei JEDEM Firmware-Commit, nicht je
Release. Ein Zweig mit zehn Commits laeuft von 2.28.4 bis 2.28.13, und ein
"git commit --amend" hebt erneut an. Wer das nicht will, nimmt den Hook aus
.pre-commit-config.yaml heraus und hebt weiter von Hand an.
"""

import pathlib
import re
import subprocess
import sys

CONFIG = pathlib.Path("Pflanzensensor/src/configs/config.h")
VERSION_RE = re.compile(r'(#define\s+VERSION\s+")(\d+)\.(\d+)\.(\d+)(")')


def staged_files():
    out = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        capture_output=True, text=True, check=True).stdout
    return [line for line in out.splitlines() if line]


def main():
    files = staged_files()

    cpp = [f for f in files
           if f.startswith("Pflanzensensor/src/") and f.endswith((".cpp", ".h", ".hpp", ".ino"))]
    if not cpp:
        return 0

    # Manuelle Anhebung hat Vorrang
    if str(CONFIG) in files:
        diff = subprocess.run(["git", "diff", "--cached", "--", str(CONFIG)],
                              capture_output=True, text=True, check=True).stdout
        if any(line.startswith("+") and "define VERSION" in line for line in diff.splitlines()):
            print("VERSION wurde von Hand geaendert - keine automatische Anhebung.")
            return 0

    text = CONFIG.read_text(encoding="utf-8")
    m = VERSION_RE.search(text)
    if not m:
        print(f"VERSION-Zeile in {CONFIG} nicht gefunden", file=sys.stderr)
        return 1

    major, minor, patch = m.group(2), m.group(3), int(m.group(4))
    neu = f"{major}.{minor}.{patch + 1}"
    CONFIG.write_text(VERSION_RE.sub(rf"\g<1>{major}.{minor}.{patch + 1}\g<5>", text, count=1),
                      encoding="utf-8")
    subprocess.run(["git", "add", str(CONFIG)], check=True)
    print(f"Version angehoben auf {neu} ({len(cpp)} geaenderte C++-Datei(en))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
