#!/usr/bin/env bash
# Stellt die Release-Dateien beider Plattformen in einem Verzeichnis zusammen
# (Standard: dist/). Vorher bauen: pio run -e esp8266_pcb -e esp32_devkit
#
#   pflanzensensor-esp8266.bin(.md5)   ganzes Abbild ab 0x0 – USB und OTA
#   pflanzensensor-esp32.bin(.md5)     Anwendung ab 0x10000 – USB und OTA
#   pflanzensensor-esp32-bootloader.bin  0x1000  (nur USB)
#   pflanzensensor-esp32-partitions.bin  0x8000  (nur USB)
#   pflanzensensor-esp32-boot_app0.bin   0xE000  (nur USB; startet app0)
#
# Der Webflasher (webflasher/build_site.py) erkennt Releases an diesen Namen.
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-dist}
mkdir -p "$out"
b8=.pio/build/esp8266_pcb
b32=.pio/build/esp32_devkit
boot_app0=${BOOT_APP0:-$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}

for f in "$b8/firmware.bin" "$b32/firmware.bin" "$b32/bootloader.bin" "$b32/partitions.bin" "$boot_app0"; do
  [ -f "$f" ] || { echo "fehlt: $f" >&2; exit 1; }
done

cp "$b8/firmware.bin" "$out/pflanzensensor-esp8266.bin"
cp "$b32/firmware.bin" "$out/pflanzensensor-esp32.bin"
cp "$b32/bootloader.bin" "$out/pflanzensensor-esp32-bootloader.bin"
cp "$b32/partitions.bin" "$out/pflanzensensor-esp32-partitions.bin"
cp "$boot_app0" "$out/pflanzensensor-esp32-boot_app0.bin"
for f in pflanzensensor-esp8266.bin pflanzensensor-esp32.bin; do
  md5sum "$out/$f" | cut -d' ' -f1 > "$out/$f.md5"
done
ls -l "$out"
