"""
PlatformIO-Post-Skript: schreibt firmware.bin.md5 neben firmware.bin.

Die Prüfsumme kann auf der Update-Seite angegeben werden ("Prüfsumme bei OTA
verlangen"); das Gerät prüft dann mit Update.setMD5().
"""
import hashlib
import pathlib

Import("env")  # noqa: F821


def write_md5(source, target, env):  # noqa: ARG001
    binary = pathlib.Path(str(target[0]))
    digest = hashlib.md5(binary.read_bytes()).hexdigest()
    pathlib.Path(str(binary) + ".md5").write_text(digest + "\n")
    print(f"MD5 {binary.name}: {digest}")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", write_md5)  # noqa: F821
