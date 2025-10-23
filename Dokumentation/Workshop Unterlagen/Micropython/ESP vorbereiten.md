# Per Commandline

https://docs.micropython.org/en/latest/esp8266/tutorial/intro.html#deploying-the-firmware

- esptool installieren
- ESP8266 per USB verbinden
- Flash löschen: `esptool.py --port /dev/ttyUSB0 erase_flash`
  - unter Windows:
- Firmware aufspielen: `esptool.py --port /dev/ttyUSB0 --baud 460800 write_flash --flash_size=detect 0 ESP8266_GENERIC-20250911-v1.26.1.bin`

# Über Thonny

- Thonny öffnen
- `Ausführen` -> `Konfiguriere den Interpreter`
- `Mikropython installieren oder aktualisieren (esptool)`
- `Target port` auswählen
- `MicroPython family` ist `ESP8266`
- `Variant` ist `Espressif ESP8266`
- `Version` die aktuellste
- dann `Installieren` klicken
