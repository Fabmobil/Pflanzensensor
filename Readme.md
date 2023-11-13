# Fabmobil Pflanzensensor

Dieser Pflanzensensor kann aus verschiedenen Modulen bestehen und kombiniert werden. Diese Module sind:

- Bodenfeuchtesensor
- Display
- Lufttemperatur und -feuchtesensor
- LED Ampel
- WiFi

Der Quellcode ist modularisiert. Folgendes ist in den unterschiedlichen Dateien zu finden:

| Dateiname | Inhalt |  |
|-----------|--------|--|
| Pflanzensensor.ino | Hauptdatei. Diese muss in der Arduino IDE geöffnet werden und läd alle anderen Dateien nach. |  |
| Configuration.h | Konfigurationsdatei: hier werden alle Einstellungen zu den Modulen usw getätigt. |  |
| modul_bodenfeuchte.h | Code für das Bodenfeuchte-Modul |  |
| modul_dht.h | Code für das Lufttemperatur und -feuchte-Modul |  |
| modul_display.h | Code für das Display-Modul |  |
| modul_ledampel.h | Code für das LED-Ampel-Modul |  |
| modul_lichtsensor.h | Code für das Lichtsensor-Modul |  |
| modul_multiplexer.h | Code für den Analogmultiplexer (wird bei gleichzeitigem Verwenden von Lichtsensor-Modul und Bodenfeuchte-Modul benötigt) |  |
| modul_wifi.h | Code für das WiFi-Modul |  |

Folgende Bibliotheken werden verwendet und müssen ggfs. in der Arduino IDE heruntergeladen werden:

| Modulname | Bibliothek(en) |
|-----------|----------------|
| Bodenfeuchte |  |
| DHT | [DHT sensor library](https://github.com/adafruit/DHT-sensor-library), [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) |
| Display | Adafruit BusIO, Adafruit GFX Library, Adafruit SSD1306 |
