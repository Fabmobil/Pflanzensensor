# ![logo_fabmobil](https://github.com/pippcat/Pflanzensensor/assets/19587872/cf56b708-fa70-4e74-826a-b1ad516401ee) Pflanzensensor

Dieser Pflanzensensor kann aus verschiedenen Modulen bestehen und kombiniert werden. Diese Module sind:

- Bodenfeuchtesensor
- Display
- Lufttemperatur und -feuchtesensor
- LED Ampel
- WiFi

## Quellcode

Der Quellcode ist modularisiert. Folgendes ist in den unterschiedlichen Dateien zu finden:

| Dateiname | Inhalt |
|-----------|--------|
| Pflanzensensor.ino | Hauptdatei. Diese muss in der Arduino IDE geöffnet werden und läd alle anderen Dateien nach. |
| Configuration.h | Konfigurationsdatei: hier werden alle Einstellungen zu den Modulen usw getätigt. |
| modul_bodenfeuchte.h | Code für das Bodenfeuchte-Modul. |
| modul_dht.h | Code für das Lufttemperatur und -feuchte-Modul. |
| modul_display.h | Code für das Display-Modul. |
| modul_display_bilder.h | Bilder, die auf dem Display angezeigt werden können. |
| modul_ledampel.h | Code für das LED-Ampel-Modul. |
| modul_lichtsensor.h | Code für das Lichtsensor-Modul. |
| modul_multiplexer.h | Code für den Analogmultiplexer (wird bei gleichzeitigem Verwenden von Lichtsensor-Modul und Bodenfeuchte-Modul benötigt). |
| modul_wifi.h | Code für das WiFi-Modul. |
| modul_wifi_bilder.h | Base64 codierte Bilder für die Webseite des Sensors. |
| modul_wifi_header.h | Der (immer gleiche) Anfang der HTML-Seiten des Sensors inklusive der CSS Formatierungsinformationen. |
| modul_wifi_footer.h | Das (immer gleiche) Ende der HTML-Seiten des Sensors. |

### Externe Bibliotheken

Folgende Bibliotheken werden verwendet und müssen ggfs. in der Arduino IDE heruntergeladen werden:

| Modulname | Bibliothek(en) |
|-----------|----------------|
| Bodenfeuchte | keine externen Bibliotheken |
| DHT | [DHT sensor library](https://github.com/adafruit/DHT-sensor-library), [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) |
| Display | [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO), [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library), [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306) |
| Helligkeit | keine externen Bibliotheken |
| LEDAmpel | keine externen Bibliotheken |
| Multiplexer | keine externen Bibliotheken |
| Wifi | ESP8266WiFi, ESP8266WebServer, ESP8266mDNS |

## Bauteile

::: info
Preise Stand 15.10.2023
:::

### Basisausstattung (8,36€)

* [NodeMCU / ESP8266](https://www.amazon.de/AZDelivery-NodeMCU-ESP8266-ESP-12E-Development/dp/B0754LZ73Z/ref=sr_1_3?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=2R1E6LL9WLQA0&keywords=nodemcu&qid=1697395604&sprefix=nodemc%2Caps%2C161&sr=8-3) : 4,40€
* [Bodenfeuchtesensor](https://www.amazon.de/KeeYees-Bodenfeuchtesensor-Kapazitive-Hygrometer-Feuchtigkeitssensor/dp/B07R174TM1/ref=sr_1_5?crid=SBQ62PDCTU01&keywords=soil+moisture+sensor&qid=1697395580&sprefix=soil+mo%2Caps%2C177&sr=8-5) : 2,66€
* [LED Ampel](https://www.amazon.de/AZDelivery-Creative-Mini-Ampel-kompatibel-Arduino/dp/B086V33MST/ref=sr_1_5?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=18271JP3Z0QGQ&keywords=led%2Bampel&qid=1697396050&sprefix=led%2Bampel%2Caps%2C138&sr=8-5&th=1) : 1,30€

### Mit Lichtsensor (+2,19€)

* [Analogmultiplexer](https://www.amazon.de/DEWIN-Electronic-Components-CD4051BE-Multiplexer/dp/B09LHTSPX9/ref=sr_1_6?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=3OFK7TJC628KS&keywords=4051+multiplexer&qid=1697395708&sprefix=4051+multiplexer%2Caps%2C126&sr=8-6) : 0,69€
* [Lichtsensor](https://www.amazon.de/AZDelivery-KY-018-Widerstand-Resistor-Arduino/dp/B07ZYXHF3C/ref=sr_1_7?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&keywords=lichtsensor&qid=1697395989&sr=8-7&th=1): 1,5€

### Mit Luchtfeuchte- und Lufttemperatursensor (+2,40€)

* [DHT11](https://www.amazon.de/AZDelivery-KY-015-DHT-Temperatursensor-Modul/dp/B089W7CJL4/ref=sr_1_4?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=1GPKR532WG8V6&keywords=dht11&qid=1697396131&sprefix=dht1%252Caps%252C144&sr=8-4) : 2,40€

### Mit Display (+4,33€)

* [1 Zoll OLED Display](https://www.amazon.de/APKLVSR-Bildschirm-Anzeigemodul-IIC-Bildschirm-kompatibel/dp/B0CFFK32S8/ref=sr_1_4?__mk_de_DE=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=2ZN5Q7U84U4GL&keywords=oled%2Barduino&qid=1697396260&sprefix=oled%2Barduino%2Caps%2C176&sr=8-4&th=1) : 4,33€
