#!/usr/bin/env python3
"""
pre-commit-Hook: hebt die Patch-Version in firmware/VERSION an, wenn C++-
Quellen der Firmware vorgemerkt sind (E10: automatisches Patch-Anheben je
Firmware-Commit, Neubau startet bei 3.0.0).

- Wurde VERSION im selben Commit von Hand geändert, passiert nichts.
- Nur Dateien unter firmware/src/ und firmware/boards/ zählen; Tests, Web,
  Skripte und CI nicht.
"""
import pathlib
import re
import subprocess
import sys

VERSION = pathlib.Path("firmware/VERSION")


def staged():
    out = subprocess.run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
                         capture_output=True, text=True, check=True).stdout
    return [l for l in out.splitlines() if l]


def main():
    files = staged()
    if str(VERSION) in files:
        print("VERSION wurde von Hand geändert - keine automatische Anhebung.")
        return 0
    cpp = [f for f in files if f.startswith(("firmware/src/", "firmware/boards/"))
           and f.endswith((".cpp", ".h", ".hpp", ".c"))]
    if not cpp:
        return 0
    text = VERSION.read_text().strip()
    m = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", text)
    if not m:
        print(f"VERSION hat kein Format X.Y.Z: {text!r}", file=sys.stderr)
        return 1
    new = f"{m.group(1)}.{m.group(2)}.{int(m.group(3)) + 1}"
    VERSION.write_text(new + "\n")
    subprocess.run(["git", "add", str(VERSION)], check=True)
    print(f"Version {text} -> {new}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
