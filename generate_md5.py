"""
Schreibt zu jedem gebauten Abbild eine .md5-Datei daneben.

Die Pruefsummen werden im Webinterface beim OTA-Upload abgefragt; steht dort
ein falscher Wert, weist das Geraet den Upload zurueck.

Warum atexit statt AddPostAction:

  Die nachgelagerten SCons-Aktionen waren nicht verlaesslich. Am Alias-Ziel
  ("buildfs") feuerte die Aktion, BEVOR das Abbild geschrieben war - die
  .md5-Datei beschrieb dann den vorigen Bau. Haengt man sie stattdessen an die
  Datei ($BUILD_DIR/littlefs.bin), feuert sie mal und mal nicht: in
  aufeinanderfolgenden, voellig gleichen Durchlaeufen einmal ja, einmal nein.
  Das laesst sich auch ohne die Asset-Komprimierung nachstellen, ist also keine
  Folge anderer Skripte hier.

  atexit haengt an nichts davon. Der Haken laeuft, wenn PlatformIO fertig ist -
  dann stehen alle Abbilder endgueltig auf der Platte. Berechnet wird die
  Pruefsumme aus der Datei selbst, sie kann also gar nicht veralten; das
  einzige Risiko waere, gar nicht zu laufen, und genau das schliesst atexit aus.
"""

import atexit
import hashlib
import os

Import("env")  # noqa: F821 - von PlatformIO bereitgestellt

IMAGES = ("${PROGNAME}.bin", "littlefs.bin")


def _md5(path):
    digest = hashlib.md5()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def write_checksums():
    build_dir = env.subst("$BUILD_DIR")  # noqa: F821
    for name in IMAGES:
        image = os.path.join(build_dir, env.subst(name))  # noqa: F821
        if not os.path.exists(image):
            continue

        checksum = _md5(image)
        target = image + ".md5"

        # Nur schreiben, wenn sich etwas geaendert hat - sonst rauscht jeder
        # Lauf die Ausgabe voll.
        if os.path.exists(target):
            with open(target, "r", encoding="ascii") as handle:
                if handle.read().strip() == checksum:
                    continue

        with open(target, "w", encoding="ascii") as handle:
            handle.write(checksum)
        print("MD5 fuer %s: %s" % (os.path.basename(image), checksum))


atexit.register(write_checksums)
