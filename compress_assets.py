"""
Legt vor dem Bau des Dateisystem-Abbilds gzip-Fassungen der Web-Assets an.

Warum: JS und CSS werden vom Gerät unkomprimiert ausgeliefert. /admin/sensors
zieht dabei rund 130 KB über WLAN von einem ESP8266, der jeweils nur eine
Verbindung bedient. Gzip drückt das auf etwa ein Viertel.

Die .gz-Dateien liegen neben den Originalen im Abbild - der Server nimmt die
komprimierte Fassung nur, wenn der Browser Accept-Encoding: gzip schickt, und
fällt sonst auf das Original zurück. Sie sind Bauartefakte und stehen deshalb
in .gitignore.
"""

import gzip
import os

Import("env")  # noqa: F821 - von PlatformIO bereitgestellt

# Dateiendungen, die sich lohnen. Bilder (png/gif/ico) sind bereits komprimiert.
SUFFIXES = (".js", ".css", ".html", ".svg")
# Unterhalb dieser Groesse spart der Aufwand nichts Nennenswertes.
MIN_BYTES = 512


def compress_assets(*_args, **_kwargs):
    data_dir = env.subst("$PROJECT_DATA_DIR")  # noqa: F821
    if not os.path.isdir(data_dir):
        return

    total_raw = total_gz = count = 0
    for root, _dirs, files in os.walk(data_dir):
        for name in files:
            if not name.endswith(SUFFIXES):
                continue
            src = os.path.join(root, name)
            dst = src + ".gz"

            raw = os.path.getsize(src)
            if raw < MIN_BYTES:
                if os.path.exists(dst):
                    os.remove(dst)
                continue

            # Nur neu erzeugen, wenn das Original neuer ist als die .gz-Fassung
            if not os.path.exists(dst) or os.path.getmtime(src) > os.path.getmtime(dst):
                with open(src, "rb") as f_in:
                    payload = f_in.read()
                # mtime=0: gleiche Eingabe ergibt gleiche Ausgabe, damit das
                # Abbild reproduzierbar bleibt.
                with gzip.GzipFile(dst, "wb", compresslevel=9, mtime=0) as f_out:
                    f_out.write(payload)

            total_raw += raw
            total_gz += os.path.getsize(dst)
            count += 1

    if count:
        print(
            "Assets komprimiert: %d Dateien, %d -> %d Bytes (%d%%)"
            % (count, total_raw, total_gz, 100 * total_gz // total_raw)
        )


env.AddPreAction("$BUILD_DIR/littlefs.bin", compress_assets)  # noqa: F821
# buildfs/uploadfs erzeugen das Abbild; beide haengen an derselben Datei.
compress_assets()
