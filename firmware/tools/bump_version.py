#!/usr/bin/env python3
"""
pre-commit-Hook: hebt die Patch-Version in firmware/VERSION an, wenn
vorgemerkte Dateien in der Firmware landen (E10: automatisches Patch-Anheben je
Firmware-Commit, Neubau startet bei 3.0.0).

- Wurde VERSION im selben Commit von Hand geändert, passiert nichts.
- Es zählen C++-Quellen unter firmware/src/ und firmware/boards/ sowie die
  Weboberfläche unter firmware/web/, soweit embed_assets.py sie einbettet
  (Endungen aus dessen TYPES) - auch eine reine Oberflächenänderung ist eine
  neue Firmware. Tests, Skripte, Webflasher und CI zählen nicht.
"""
import pathlib
import re
import subprocess
import sys

VERSION = pathlib.Path("firmware/VERSION")
EMBED = pathlib.Path("firmware/tools/embed_assets.py")


def web_suffixes():
    """Endungen, die embed_assets.py einbettet (TYPES), ohne es zu importieren
    (das Skript läuft beim Import als PlatformIO-Vorstufe los)."""
    block = re.search(r"^TYPES = \{(.*?)^\}", EMBED.read_text(), re.S | re.M)
    return tuple(re.findall(r'"(\.\w+)"\s*:', block.group(1))) if block else ()


def counts(path, suffixes):
    """Landet diese Datei in der Firmware?"""
    if path.startswith(("firmware/src/", "firmware/boards/")):
        return path.endswith((".cpp", ".h", ".hpp", ".c"))
    return path.startswith("firmware/web/") and path.endswith(suffixes)


def staged():
    out = subprocess.run(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
                         capture_output=True, text=True, check=True).stdout
    return [l for l in out.splitlines() if l]


def main():
    files = staged()
    if str(VERSION) in files:
        print("VERSION wurde von Hand geändert - keine automatische Anhebung.")
        return 0
    suffixes = web_suffixes()
    if not suffixes:
        print(f"Keine TYPES-Tabelle in {EMBED} gefunden.", file=sys.stderr)
        return 1
    if not any(counts(f, suffixes) for f in files):
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
