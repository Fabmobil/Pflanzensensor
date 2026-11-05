"""
PlatformIO-Pre-Skript: verhindert buildfs/uploadfs.

Firmware 3 hat kein Dateisystem-Image mehr (refactor.md E6): Die Weboberfläche
ist in firmware.bin eingebettet, LittleFS enthält nur Gerätedaten
(Konfiguration, Kanalzustand, Chronik, Log). Ein hochgeladenes Image würde
genau diese Daten löschen. Deshalb wird hier mit einer klaren Meldung
abgebrochen statt mit "can't read source directory".
"""
import sys

Import("env")  # noqa: F821

FS_TARGETS = {"buildfs", "uploadfs", "uploadfsota", "erasefs"}

if FS_TARGETS & set(COMMAND_LINE_TARGETS):  # noqa: F821
    sys.stderr.write(
        "\n"
        "Firmware 3 braucht kein Dateisystem-Image.\n"
        "  - Die Weboberfläche steckt in firmware.bin: einfach \"Upload\" (pio run -t upload).\n"
        "  - LittleFS enthält nur Gerätedaten (Konfiguration, Chronik, Log) und wird beim\n"
        "    ersten Start automatisch angelegt. Ein Image würde diese Daten löschen.\n"
        "  - Alles zurücksetzen: Admin-Seite > Werksreset (Chronik bleibt) oder\n"
        "    pio run -t erase (löscht den kompletten Flash).\n\n")
    env.Exit(1)  # noqa: F821
