# Pflanzensensor – Analyse und Modell für die Neuimplementierung

Stand: 2026-11-04 · Grundlage: Branch `refactor`, Firmware-Version 2.31.6 (`Pflanzensensor/src`, `Pflanzensensor/data`, `test/`, Build-Skripte, CI)

Dieses Dokument ist der Auftrag für die Neuentwicklung. Ein Agent soll damit und mit dem alten Code als Referenz die komplette Firmware neu bauen. Der alte Code beantwortet die Frage „**was** tut das Gerät“ und zeigt die Fallstricke der Hardware. Das „**wie**“ ist frei: Architektur, Verzeichnisstruktur, Datenformate und interne APIs werden neu entworfen, außer an den drei ausdrücklich kompatiblen Schnittstellen (Abschnitt 3).

Aufbau:

1. Leitplanken und getroffene Entscheidungen
2. Funktionaler Soll-Umfang (was die neue Firmware können muss)
3. Externe Schnittstellen, die kompatibel bleiben
4. Befunde im Altcode (Fehler, toter Code, ungünstige Muster)
5. Zielarchitektur
6. Arbeitspakete, Reihenfolge und Abnahmekriterien
7. Offene Punkte und Annahmen
8. Anhang: Referenzkarte Altcode → Funktion

---

## 1. Leitplanken und Entscheidungen

| # | Entscheidung | Quelle |
|---|---|---|
| E1 | Zielhardware ist das eigene PCB mit **ESP8266 (NodeMCU v2, 4 MB Flash)**. Der Code muss zusätzlich auf **ESP32** bauen und laufen. | Nutzer |
| E2 | Alles darf neu sein: Architektur, Verzeichnisse, Datenformate, interne APIs. Eine Migration alter Dateien auf dem Gerät gibt es nicht, **aber eine mit der alten Firmware heruntergeladene Konfigurationsdatei (`config.json`) muss importiert werden können** (siehe 2.3, „Import alter Konfiguration“). | Nutzer |
| E3 | **Alle acht Sensortypen** werden voll unterstützt: DHT, Analog (mit Multiplexer), DS18B20, SDS011, MH-Z19, HX711, BMP280, Serial Receiver. Die Hardware für alle ist vorhanden. **Vorrang haben die Standardsensoren Analog und DHT**; die übrigen folgen danach (AP-13), die Architektur muss sie aber von Anfang an tragen. | Nutzer |
| E4 | Oberstes Qualitätsziel: **wenig RAM und hohe Stabilität, ohne Feature-Verlust**. | Nutzer |
| E5 | Ziel ist modernes, effektives C++ in einer sauberen Architektur, funktional gleichwertig zur alten Firmware. | Nutzer |
| E6 | **Web-Assets werden gzip-komprimiert in die Firmware eingebettet.** Updates bestehen nur noch aus `firmware.bin`. LittleFS enthält ausschließlich Daten und wird nie per OTA überschrieben. Die gesamte Flash-Sicherung (FlashPersistence, Restore-Flag, Backup-Bereich hinter dem Sketch) entfällt. | Nutzer |
| E7 | **Statische Seiten + JSON-API.** HTML, JS und CSS sind statisch; das Gerät liefert Daten nur als JSON (bzw. als Binärstrom für die Chronik). Es gibt kein serverseitiges HTML-Rendering mehr. | Nutzer |
| E8 | Kompatibel bleiben: **Mailbot-Protokoll**, **Prometheus-Metriknamen** und **`GET /getLatestValues`**. | Nutzer |
| E9 | Sicherheit: **Das fest einkompilierte Notfall-Adminpasswort bleibt.** Alle schreibenden Endpunkte (auch Debug-Schalter und Log-Level) und das Live-Log nur mit Login. Passwörter und Schlüssel werden nie an den Browser zurückgegeben und nicht exportiert. Lesend öffentlich bleiben Startseite, `/getLatestValues`, `/api/v1/measure`, Chronik, `/metrics`, `/status` und `/api/v1/system`. Der AP bleibt offen, bekommt aber ein Captive-Portal. | Nutzer |
| E10 | Admin-Passwort ohne Mindestlänge; Chronik übersteht den Werksreset; Versionierung bleibt (automatisches Patch-Anheben je Firmware-Commit, Neubau startet bei 3.0.0); kein Alias für den alten Pfad `/measure`. | Nutzer |

Die eingebetteten Assets kosten etwa 72 KB (JS/CSS, gzip -9) plus 37 KB Bilder, zusammen rund 110 KB. Das ist bei 1575 KB pro OTA-Slot unkritisch.

---

## 2. Funktionaler Soll-Umfang

Jede Funktion bekommt eine ID (F-xx), auf die Arbeitspakete und Tests verweisen. Konstanten sind die Werte der alten Firmware und gelten als Vorgabe, solange nichts anderes steht. Mit „Altcode“ markierte Stellen zeigen auf die Referenzimplementierung.

### 2.1 Hardware und Plattform (F-01)

**PCB** (Schaltplan `Dokumentation/Pflanzensensor PCB/Pflanzensensor_Schematic.pdf`, Stand 2024-06-06):
NodeMCU-Board, CD4051B-Multiplexer, DHT11 mit 10-kΩ-Pull-up, Fotowiderstand mit 1-kΩ-Teiler, LED-Ampel (rot 68 Ω, gelb 62 Ω, grün 56 Ω), Display-Stecker (GND, VCC, SCL, SDA), drei dreipolige Bodenfeuchte-Stecker, weitere Multiplexer-Kanäle auf Stiftleisten, USB-C-Versorgung.

**Standardbelegung** (aus `config_example.h`; muss pro Board-Profil festgelegt werden):

| Funktion | GPIO (ESP8266) | Bemerkung |
|---|---|---|
| Analogeingang | A0 | 10 Bit, 0..1023 |
| Mux-Adresse A (LSB) / B / C (MSB) | 15 / 2 / 16 | GPIO15 und GPIO2 sind Boot-Strapping-Pins |
| DHT | 0 (D3) | ebenfalls Strapping-Pin |
| LED grün / gelb / rot | 14 / 12 / 13 | aktiv HIGH |
| I²C (Display, BMP280) | SDA 4, SCL 5 | Wire-Standardpins; die Config-Werte `DISPLAY_RX/TX_PIN` werden im Altcode nie benutzt |
| Display | SSD1306 128×64, Adresse 0x3C | |
| Optional DS18B20 | 4 | Konflikt mit SDA |
| Optional SDS011 RX/TX | 12 / 13 | Konflikt mit LED gelb/rot |
| Optional MH-Z19 PWM | 14 | Konflikt mit LED grün |
| Optional HX711 SCK/DOUT | 5 / 4 | Konflikt mit I²C |
| Optional Serial Receiver RX/TX | 14 / 12 | Konflikt mit LEDs |

**Anforderung:** Pinbelegungen leben in einem Board-Profil (Abschnitt 5.4). Konflikte werden **zur Compile-Zeit** geprüft (`static_assert` über alle aktiven Ressourcen). Beim Multiplexer ist die Adressierung **invertiert**: Kanal n (1-basiert) bekommt die Adresse `7-(n-1)`; Kanal 1 entspricht A=B=C=HIGH.

**Flash-Layout ESP8266** (`eagle.flash.4mmore.ld`): Sketch 1575 KB, OTA-Slot 1575 KB, LittleFS ab 0x40512000 mit etwa 950 KB, **Blockgröße 8 KB** (jedes Verzeichnis kostet zwei Blöcke, also 16 KB), danach EEPROM 4 KB, RF-Cal und WLAN-Bereich. Das Layout kann bleiben. Auf ESP32 kommt eine eigene Partitionstabelle mit zwei OTA-Slots und LittleFS.

**Plattformgrenzen ESP8266, die die Architektur bestimmen:**

- etwa 80 KB DRAM, davon typischerweise 15 bis 25 KB freier Heap im Betrieb
- **Loop-Stack nur 4 KB** (Altcode: `check_stack.sh` misst deshalb Stapelrahmen)
- kooperatives Multitasking: `yield()` außerhalb des cont-Kontexts löst eine Panic aus
- Flash-Erase/-Write mit gesperrten Interrupts lässt den WLAN-Stack verhungern
- ein TLS-Handshake braucht rund 10 bis 12 KB am Stück; das ist der Grund für den unverschlüsselten Mailbot-Transport

### 2.2 Systemstart, Wiederanlauf, Hauptschleife (F-02)

**Bootfolge** (Altcode `main.cpp`, `main_*.cpp`):

1. Serial 115200.
2. LittleFS einhängen. Schlägt das fehl, formatieren und das als Fehler melden; der Altcode bricht hier stattdessen ab.
3. Boot-Schleifen-Erkennung.
4. Konfiguration laden.
5. Neustartzähler erhöhen.
6. Display und Bootbildschirm.
7. WLAN (STA mit drei Slots, sonst AP).
8. NTP.
9. LED-Ampel.
10. Sensoren.
11. Chronik.
12. Webserver, WebSocket, mDNS.
13. Mailversand.
14. Bootbildschirm beenden.

**Boot-Schleifen-Schutz:** Der Altcode zählt Starts innerhalb der ersten 60 s Laufzeit in `/.boot_loop`. Ab drei Starts schaltet er das Datei-Logging ab, löscht `/log.txt` und hebt den Update-Modus auf. Ab zwei Starts zeichnet die Chronik in diesem Lauf nicht auf. Der Zähler wird nach 60 s stabiler Laufzeit gelöscht.

Soll: Dieselbe Wirkung, aber robust gelöst. Der Zähler liegt im RTC-RAM (ESP8266 `system_rtc_mem`, ESP32 `RTC_NOINIT_ATTR`) und nicht in einer Flash-Datei. Ein Kaltstart (Power-on) gilt nicht als Schleife; dafür den Reset-Grund auswerten.

**Begrenzte Sensor-Neustarts:** Der DS18B20 darf das Gerät bei dauerhaftem Fehler neu starten, aber höchstens dreimal in Folge. Der Zähler liegt in `.noinit`-RAM und wird nach 10 min stabiler Laufzeit zurückgesetzt.

**Hauptschleife:** Sie ist kooperativ und blockiert nie länger als etwa 50 ms. Takte:

| Takt | Aufgabe |
|---|---|
| jede Schleife | Webserver, WebSocket, mDNS-Update |
| 1 s | Messzyklus aller Sensoren (bei manuell ausgelöster Messung jede Schleife) |
| 1 s | LED-Ampel |
| `screen_dur` | Display-Rotation |
| 30 s | WLAN-Prüfung bzw. AP-Rückweg |
| 30 s | Speicherüberwachung |
| 1 h | NTP-Resync; ohne gültige Zeit jede Minute |
| 30 s, bei Testmail 5 s | Mail-Scheduler |
| „wenn fällig“ | Flush von Log, Chronik und Sensorzustand, **ausschließlich hier** |

**Grundregel aus dem Altcode, verbindlich:** Flash-Schreibzugriffe, TLS bzw. HTTP-Clients und andere lange Operationen laufen **nur aus der Hauptschleife**. Nie aus Web-Handlern, Callbacks oder Logger-Aufrufen. Handler legen Aufträge nur in eine Queue.

### 2.3 Konfiguration (F-03)

**Gerätekonfiguration** (zur Laufzeit änderbar, persistent):

| Bereich | Einstellung | Typ / Grenzen | Standard |
|---|---|---|---|
| Allgemein | Gerätename | UTF-8, ≤ 32 Byte (UI) | „Fabmobil Pflanzensensor“ |
| | Admin-Passwort | nicht leer, ≤ 32 Zeichen, ASCII 0x20..0x7E, **keine Mindestlänge** (E10; Client und Server prüfen identisch, Befund B-49) | „Fabmobil“ (Build-Konstante) |
| | Prüfsumme bei OTA verlangen | bool | aus |
| | Blumengesicht folgt Kanal | Kanalschlüssel | `ANALOG_1` |
| WLAN | 3 Slots: SSID, Passwort | SSID ≤ 32, Passwort ≤ 64 | aus Build-Profil, im Release leer |
| Display | Dauer pro Bildschirm | 1..60 s | 5 s |
| | Uhrformat | 24h \| 12h | 24h |
| | IP-, Uhr-, QR-, Blumen-, Fabmobil-Bildschirm | bool | an, an, aus, an, an |
| | Kanal auf Display zeigen | bool je Kanal | an |
| LED-Ampel | Modus | 0 aus, 1 folgt Display, 2 fester Kanal | 2 |
| | Fester Kanal | Kanalschlüssel | `ANALOG_1` |
| | Nur Rot | bool | aus |
| Logging | Log-Level | DEBUG \| INFO \| WARNING \| ERROR | INFO |
| | Datei-Log | bool | aus |
| | Debug-Schalter | RAM, Messzyklus, Sensor, Display, WebSocket, Mail | aus |
| Mail | siehe F-11 | | |

**Sensor- und Kanalkonfiguration** (persistent):

| Ebene | Einstellung | Bemerkung |
|---|---|---|
| Sensor | aktiviert | wird im Altcode **nicht** persistiert (Befund B-12) |
| Sensor | Messintervall | 10..3600 s, Standard 60 s; SDS011 600 s |
| Kanal | aktiviert, Name, Einheit, Feldname | Feldname wird für Prometheus und `/getLatestValues` gebraucht |
| Kanal | Schwellen yellowLow ≤ greenLow ≤ greenHigh ≤ yellowHigh | float, Nachkommastellen erhalten (Befund B-30) |
| Analog-Kanal | Roh-Min, Roh-Max, invertiert | |
| Analog-Kanal | Autokalibrierung an/aus, Halbwertszeit | 6 h / 12 h / 1 d / 3 d / 7 d / 30 d, Standard 1 d |
| HX711 | Skalenfaktor, Tara-Offset | Offset persistieren statt bei jedem Init neu zu tarieren (Befund B-21) |

**Laufzeit-Zustand je Kanal** (überlebt Neustart, wird aber gedrosselt geschrieben): letzter Wert, letzter Rohwert, beobachtetes Min/Max, beobachtetes Roh-Min/Max, Autokalibrierungsgrenzen (Float-Zustand).

**Standardwerte der Kanäle** (aus `config_example.h`):

| Kanal | Name | Feldname | Einheit | Schwellen yl / gl / gh / yh | Sonstiges |
|---|---|---|---|---|---|
| DHT_0 | Lufttemperatur | lufttemperatur | °C | 10 / 15 / 25 / 30 | |
| DHT_1 | Luftfeuchte | luftfeuchte | % | 20 / 30 / 80 / 90 | |
| ANALOG_0 | Lichtstärke | lichtstaerke | % | 10 / 20 / 80 / 90 | Roh 5..1023, nicht invertiert |
| ANALOG_1..7 | Bodenfeuchte, Bodenfeuchte 2..7 | bodenfeuchte, bodenfeuchte_2..7 | % | 10 / 20 / 80 / 90 | Roh 250..750, invertiert |
| DS18B20_n | DS18B20_n | ds18b20_n | °C | −10 / 0 / 40 / 60 | |
| SDS011_0 / _1 | PM10 / PM2.5 | pm10 / pm25 | µg/m³ | einseitig: gh 25 / yh 50 bzw. gh 15 / yh 35 | |
| MHZ19_0 | CO2 | co2 | ppm | 400 / 600 / 1200 / 2000 | gültig 0 < v ≤ 5000 |
| HX711_0 | Gewicht | gewicht | g | 0 / 10 / 5000 / 10000 | Skala −7050 |
| BMP280_0 / _1 | BMP_Temperatur / Luftdruck | bmp280_temperatur / luftdruck | °C / hPa | −10 / 10 / 35 / 50 bzw. 950 / 980 / 1030 / 1060 | |
| SERIAL_RECEIVER_0..6 | Flow Rate, Absolute Counts, Sum Flow Rate, 24h Flow Rate, Arduino Time, Uptime, Liters per Hour | l_per_min, absolute_counts, sum_l_per_min, l_per_min_24h, arduino_millis, uptime, l_per_hour | l/min, counts, l/min, l/min, ms, s, l/h | siehe `sensor_serial_receiver.cpp` | Gültigkeitsbereiche dort |

**Kanalschlüssel:** `<SENSOR-ID>_<Index>` mit 0-basiertem Index, etwa `ANALOG_1` oder `DHT_0`. Sensor-IDs: `ANALOG`, `DHT`, `DS18B20`, `SDS011`, `MHZ19`, `HX711`, `BMP280`, `SERIAL_RECEIVER`. Der Schlüssel taucht in `/getLatestValues`, in der Chronik-Kanaltabelle, in der Mail-Auswahl, bei der LED-Auswahl und beim Blumensensor auf. Er bleibt so (E8).

**Werksreset:** Er löscht die komplette Konfiguration und den Kanalzustand und startet neu. Danach gelten die Build-Standards; das Gerät öffnet ggf. den AP. **Die Chronik bleibt erhalten** (E10); das Datei-Log ebenfalls.

**Konfig-Export/-Import:** Download als JSON-Datei, Upload derselben Datei, danach Neustart. Zusätzlich wird das alte Format akzeptiert (siehe unten). Das Format ist neu und versioniert (`"schema": 1`). Es enthält alle Einstellungen aus der Gerätekonfiguration, die Sensor- und Kanalkonfiguration und die Mailvorlagen; Messdaten nicht. Geheimnisse (Passwörter, Mail-Schlüssel) sind ausgenommen (E9).

**Import alter Konfiguration (E2):** Die Datei aus „Konfiguration herunterladen“ der alten Firmware (`config.json`, erzeugt von `ConfigPersistence::backupPreferencesToFile`) wird beim Import erkannt, weil `"schema"` fehlt und `"general"` oder `"sensors"` vorhanden ist. Sie wird so übernommen:

| Alt (Pfad im JSON) | Neu | Bemerkung |
|---|---|---|
| `general.device_name` | Gerätename | |
| `general.admin_pwd` | Admin-Passwort | als Hash speichern |
| `general.md5_verify` | Prüfsumme bei OTA verlangen | |
| `general.file_log`, `log.file_enabled` | Datei-Log | `general.file_log` hat Vorrang |
| `general.flower_sens` | Blumengesicht-Kanal | |
| `wifi.ssid1..3`, `wifi.pwd1..3` | WLAN-Slots 1..3 | Passwörter übernehmen |
| `display.show_ip/show_clock/show_flower/show_fabmobil/show_qr` | Bildschirm-Schalter | |
| `display.screen_dur` | Anzeigedauer | Altwert in ms; Werte < 1000 als Sekunden deuten (Altcode-Default 5, Befund B-04) |
| `display.clock_fmt` | Uhrformat | |
| `display.sensor_disp` | Kanal auf Display | Format `ID:1,0,…;ID2:…` |
| `debug.ram/meas_cycle/sensor/display/websocket` | Debug-Schalter | `debug.mail` fehlt im Altexport → aus |
| `log.level` | Log-Level | |
| `led_traffic.mode/sel_meas/only_red` | LED-Ampel | |
| `sensors[].id`, `sensors[].measInt` | Sensor, Intervall | ms |
| `sensors[].measurements[]` mit `idx, en, nm, fn, un, min, max, thresh{yl,gl,gh,yh}` | Kanal `<id>_<idx>`: aktiviert, Name, Feldname, Einheit, Roh-Min/Max (nur Analog), Schwellen | |
| Analog zusätzlich `inv, cal, ahl, rmin, rmax, amin, amax, lastRawValue`, alle `lastValue` | invertiert, Autokal, Halbwertszeit, Roh-Extrema, Wert-Extrema (`null` = unbekannt), letzter Wert/Rohwert | Zustand in `state.bin` |
| Altstruktur ohne `measurements` (Gruppe = Messung, Felder direkt) | wie oben | vom Altcode ebenfalls gelesen |

Die Mail-Einstellungen fehlen im Altexport (Befund B-04) und bleiben auf Standard. Sensoren, die das Board-Profil nicht kennt, und unbekannte Schlüssel werden ignoriert und im Ergebnis gemeldet. Der Import liefert eine Zusammenfassung (übernommen / ignoriert / ungültig) und startet danach neu. Testdateien: Ein Export der alten Firmware (mit Platzhalter-Passwörtern) kommt nach `test/vectors/legacy_config_*.json`.

### 2.4 Sensoren und Messzyklus (F-04)

**Begriffe:** Ein *Sensor* ist ein Hardwaretreiber mit einer ID und einem eigenen Intervall. Ein *Kanal* ist eine Messgröße des Sensors (Index 0..7, `MAX_CHANNELS_PER_SENSOR = 8`). Über alle Sensoren gibt es höchstens 16 aktive Kanäle; diese Grenze kommt aus Chronik und Mail.

**Messmodell im Neubau** (Entscheidung O11). Der Altcode hat pro Sensor eine Zustandsmaschine mit 9 Zuständen, die um einen globalen Messslot konkurrieren (Altcode `sensor_measurement_cycle*.cpp`, `sensor_manager_limiter.h`). Das daraus folgende Geflecht aus Slot-Wartezeit (40 s), Zwangsfreigabe (45 s), „Slot verloren“-Erkennung, sechs Zustands-Zeitschranken und Preemption entfällt. **Fachlich gleich bleiben:** nicht-blockierendes Messen, Mehrfach-Samples, keine gleichzeitige Nutzung geteilter Hardware, Warmup, Fehler-Wiederholung mit Backoff, Sofortmessung. Neu gebaut wird es so:

1. **Ein zentraler Mess-Scheduler** (`core/sensors/scheduler`) kennt alle Sensoren und ihre nächste Fälligkeit und führt *Messaufträge* aus. Er allein verwaltet die geteilten Ressourcen; ein Auftrag kann einen „Slot“ nicht verlieren oder festhalten.
2. **Ressourcen statt globalem Slot:** Jeder Treiber meldet eine Ressourcenmaske (`Adc` für Analog/Mux, `I2c`, `OneWire`, `SoftUart`, `None` für eigene Pins wie DHT). Belegt wird eine Ressource **nur während einer Sample-Runde**, nicht beim Aufwärmen und nicht zwischen den Runden. Runden verschiedener Sensoren laufen parallel, wenn sich ihre Masken nicht überschneiden; sonst nacheinander.
3. **Ein Messauftrag hat drei Phasen:**

   ```
   Prepare ──> Sample (rounds × [Runde, roundDelay]) ──> Finish
     begin()/wake(),                                         Median je Kanal, Gültigkeit,
     Warmup abwarten (ohne Ressource)                        sleep(), Status, Ereignis "Messung fertig"
   ```

   Es gibt **eine Frist pro Auftrag**: `warmup + rounds × (roundDelay + roundTimeout) + 5 s`. Wird sie überschritten, bricht der Scheduler ab, gibt Ressourcen frei, ruft `sleep()` und zählt einen Fehlschlag.
4. **Sampling in Runden:** Eine Runde liest **alle Kanäle** des Sensors (Analog: Mux umschalten, ~1 ms settle, ADC lesen, für jeden Kanal; DHT: eine Lesung liefert beide Kanäle; DS18B20: eine Konversion für alle Geräte). Standard sind 3 Runden. Pro Kanal zählt der **Median** der gültigen Runden; bei 2 gültigen der Mittelwert, bei einer dieser Wert. Rundenabstände: Analog 300 ms, DHT 1000 ms (DHT11) bzw. 2000 ms (DHT22), DS18B20 0 (Konversion 750 ms ist Teil der Runde), MH-Z19 2000 ms, SDS011 1000 ms, HX711 100 ms, BMP280 100 ms, Serial Receiver 0. Eine Analog-Messung mit 8 Kanälen dauert so ~1 s statt ~12 s im Altcode.
5. **Warmup:** *Einmalig nach dem Start* (MH-Z19 180 s) bedeutet, dass der Sensor frühestens nach Ablauf erstmals fällig ist und den Status `warmup` hat. *Pro Messung* (SDS011 30 s nach `wake()`, DHT 0) ist Teil von *Prepare*. In beiden Fällen ist keine Ressource belegt, andere Sensoren messen ungehindert (behebt B-19).
6. **Taktung ohne Drift:** Nächste Fälligkeit = geplanter Start + Intervall. Ist ein Auftrag mehr als ein Intervall verspätet, wird auf `jetzt + Intervall` gesetzt (kein Nachholen). Beim Start werden die Sensoren um je 2 s versetzt erstmals fällig. Es gibt **keine Ausrichtung auf volle Minuten** (Nutzerentscheidung). Alle Zeitvergleiche sind überlaufsicher (B-07).
7. **Sofortmessung** ist eine **Priorität**, kein Abbruch: Der Sensor wird sofort fällig und rückt an den Anfang der Warteschlange. Läuft für ihn schon ein Auftrag, wird nichts neu gestartet (Antwort `Busy`). Ein laufendes Start-Warmup wird respektiert (Antwort `NotReady` mit Restzeit, wird geloggt). Die öffentliche Drossel bleibt bei 2 s global (`RequestThrottle`).
8. **Fehlermodell pro Sensor:** `Ok → Degraded → Failed`.
   - Ein fehlgeschlagener Auftrag (Prepare-Fehler, Frist, kein Kanal gültig) erhöht `consecutiveFailures` und plant nach 5 s erneut (bis zu 4-mal).
   - Ab 5 Fehlschlägen in Folge ist der Sensor `Failed`: Alle Kanäle bekommen den Status `error`, Wiederholung mit Backoff `Intervall × 2^(Stufe−1)`, höchstens 30 min, Stufe ≤ 8.
   - Jeder Auftrag nach einem Fehlschlag beginnt mit `begin()`; Treiber müssen `begin()` beliebig oft vertragen.
   - Eine erfolgreiche Messung setzt alles zurück.
   - DS18B20: begrenzter Geräteneustart wie in F-02, ausgelöst beim Übergang nach `Failed`.
9. **Gültigkeit pro Kanal:** Plausibilitätsbereich je Kanal. Ein Auftrag ist erfolgreich, wenn mindestens ein Kanal gültig ist. Ungültige Kanäle bekommen „kein Wert“ (NaN) und den Status `error`, gültige ihren Wert. Es wird nie 0 als Ersatz gespeichert (B-08), und Indizes verschieben sich nicht (B-09).
10. **Finish:** Werte setzen, Status berechnen, beobachtetes Min/Max nachführen, letzten Wert merken (Zustand im `StateStore`, Speicherung gedrosselt, 5.7), Ereignis `MeasurementDone{sensor}` an Chronik, Mail-Scheduler, LED und Display.
11. **Statusberechnung** (Regeln aus `test_messzustand`):
    - zweiseitig: `v < yl || v > yh` → rot; `v < gl || v > gh` → gelb; sonst grün
    - einseitig (SDS011): `v ≤ gh` → grün; `v ≤ yh` → gelb; sonst rot
    - weitere Status: `error` (kein gültiger Wert bzw. `Failed`), `warmup` (Start-Warmup läuft), `unknown` (noch nie gemessen)
    - Ob ein Kanal einseitig ist, ist eine Eigenschaft der Kanaldefinition, nicht des ID-Präfixes (B-20).
12. **Sensor, dessen `begin()` beim Start fehlschlägt**, bleibt in der Liste und durchläuft das Fehlermodell (B-10). **Aktivieren/Deaktivieren** wirkt sofort und wird persistiert (B-11, B-12); ein deaktivierter Sensor bekommt keine Aufträge, ein laufender wird sauber beendet.

**Treiber** (Details siehe Altcode-Dateien; die folgenden Punkte sind die fachlich relevanten):

| Sensor | Kanäle | Hardware/Protokoll | Besonderheiten |
|---|---|---|---|
| Analog | 1..8 (`ANALOG_SENSOR_COUNT`) | A0, optional CD4051B | Pro Runde alle Kanäle: Mux umschalten, ~1 ms settle (Altcode 500 µs; auf Hardware verifizieren), Pins zurücklesen und einmal wiederholen. Wert: Rohwert auf [min,max] begrenzen (ohne Autokalibrierung, Warnung einmal pro Zyklus), `pct = 100·(raw−min)/(max−min)`, invertiert `100·(max−raw)/(max−min)`, `max == min` ergibt 0; gültig 0..100. Den Rohwert des letzten Samples merken; die Roh-Extrema beim ersten Wert setzen und danach nur erweitern. |
| Autokalibrierung (Analog) | | EMA | Rohwert außerhalb [min,max] setzt die jeweilige Grenze sofort. Sonst „Vergessen“ per EMA: `α = 1 − 0.5^(1/n)`, `n = Halbwertszeit_s·1000/Intervall_ms`; Fallback `α = 1e-4` bei ungültigen Werten. Der Float-Zustand wird separat gehalten; persistiert wird nur bei Änderung der gerundeten Ganzzahl. Bei min > max tauschen. Beim Einschalten: Start aus konfiguriertem min/max, erweitert um den letzten Rohwert. Im Autokal-Modus sind min/max manuell gesperrt. |
| DHT | 2 (Temp −40..80, Feuchte 0..100) | DHT11/22 | Typ aus Profil; eine Lesung pro Runde liefert beide Kanäle; Rundenabstand ≥ 1 s (DHT11) bzw. 2 s (DHT22); Präsenz erkennen (der Altcode meldet immer Erfolg). |
| DS18B20 | 1..8 | OneWire, DallasTemperature | Geräte zählen; weniger gefunden → weniger Kanäle. 10 Bit Auflösung. **Eine** Konversion für alle Sensoren (Befund B-18), danach 750 ms warten. Gültig −55..125, −127 und DISCONNECTED sind ungültig. |
| SDS011 | 2 (PM10, PM2.5), einseitig | UART 9600 (SoftwareSerial auf ESP8266) | 19-Byte-Kommando: AA B4 cmd d1 d2 00×10 FF FF cksum AB. 10-Byte-Antwort: AA C5/C0 … cksum AB. Query 0x04, Sleep/Work 0x06 (data 01 = Work, 00 = Sleep). PM2.5 = (b2+256·b3)/10, PM10 = (b4+256·b5)/10. **Wecken → Warmup 30 s (ohne Ressource) → 3 Runden → Schlafen** pro Messung. |
| MH-Z19 | 1 (CO₂ ppm) | PWM-Ausgang | `ppm = range·(th_ms−2)/(th_ms+tl_ms−4)`, range 2000 oder 5000 aus dem Profil (Befund B-20); Start-Warmup 180 s. Nicht-blockierend über Flankeninterrupt messen, nicht per `pulseIn` (Befund B-17). |
| HX711 | 1 (Gewicht g) | HX711 | Skala aus Konfig; Tara nur auf Anforderung (neue Admin-Aktion „Tarieren“), Offset persistent; `power_down` nach der Messung. |
| BMP280 | 2 (Temp −40..85 °C, Druck 300..1100 hPa) | I²C 0x76 | Sampling normal, T×2, P×16, IIR 16, Standby 500 ms. |
| Serial Receiver | 7 | UART, Zeilenprotokoll | Puffer leeren, `GET:<idx>\n` senden, Antwortzeile (Timeout 4 s) mit `strtof` parsen, Bereichsprüfung je Kanal. Nicht-blockierend über Zeilenpuffer umsetzen. Baudrate und Timeout aus dem Profil. |

**Hinweis Half-Float:** Die Chronik speichert Werte als IEEE-half. Das reicht für Prozent und °C. Luftdruck hat bei rund 1000 hPa eine Auflösung von 0,5 hPa, und Gewichte über 65504 g werden ±Inf. Der Neubau speichert in der Chronik je Kanal skalierte `int16` (Skala und Offset in der Kanaltabelle) oder `float32` bei großem Wertebereich (siehe 5.8).

### 2.5 Startseite und öffentliche Live-Ansicht (F-05)

Statische Seite `/`. Daten kommen per Polling aus `/getLatestValues` (E8) bzw. aus der neuen API. Funktionen (Altcode `startpage_handler.cpp`, `sensors.js`, getestet in `sensors.test.mjs`):

- Kopf „Wolke“ mit dem Gerätenamen; die Wolke schwebt sanft (Sinus x ±30 px, y ±8 px).
- Blume mit Gesicht nach dem Status des Blumen-Kanals: grün → `face-happy.gif`, gelb → `face-neutral.gif`, rot → `face-sad.gif`, sonst → `face-error.gif`. Der Hintergrundverlauf der Box folgt dem Status (Klassen `status-*`); die Karte des Blumen-Kanals bekommt `.active`.
- Je aktivierter Kanal eine „Blatt“-Karte, abwechselnd links und rechts: NAME in Versalien, Wert mit 1 Nachkommastelle plus Einheit bzw. „--“, „STATUS: OK/Warnung/Kritisch/Fehler/Aufwärmen/Unbekannt“ (einheitliche Schreibweise festlegen; Befund B-52), „(Xs/Ys)“ für Sekunden seit der Messung und das Intervall, sekündlich lokal hochgezählt.
- Poll alle 10 s, nur bei sichtbarem Tab, nie parallel. Nach drei Fehlern Fehlerzustand.
- Klick, Enter oder Leertaste auf eine Karte löst eine Sofortmessung des Sensors aus (öffentlich): Alle Karten des Sensors gehen in den Zustand „(misst …)“. Danach folgen Schnell-Polls nach 1,5/2/2,5/3/4/5 s, bis ein neuer Messzeitpunkt da ist. Dann „wackelt“ das Blatt 700 ms. Notbremse nach 25 s: „(keine Antwort)“. Bei 429 erscheint „(bitte warten)“, bei Fehler „(Fehler)“, jeweils 3 s lang.
- Fußzeile (auf allen Seiten): Navigation START / CHRONIK / LOGS / ADMIN und eine Statustabelle mit Zeit, SSID, NAME (`<mdns>.local`), IP, WLAN-RSSI in dBm, Laufzeit und Neustarts. Auf Admin-Seiten bzw. beim Klick auf ADMIN auf der Startseite erscheint stattdessen das Admin-Menü: Einstellungen, Sensoren, Display (nur mit Display), E-Mail, Update. Darunter Fabmobil-Logo (Link https://www.fabmobil.org), Version und Build-Datum.
- Optik: Pixel-Look; CSS und Bilder werden übernommen (`data/css`, `data/img`).

### 2.6 Chronik (Messwert-Historie) (F-06)

Altcode `chronik/*`, `utils/chronik_*.h`, `chronik_handler.cpp`, `chronik.js`; gut getestet. Fachlich:

- Nach jeder fertigen Messung wird ein Datensatz mit Zeit, Kanal, Wert, Status und (bei Analog) Rohwert aufgezeichnet. **Nur mit plausibler Uhrzeit** (Epoch ≥ 1600000000); sonst wird verworfen und gezählt, höchstens ein Log pro Stunde.
- Speicherung in Segmentdateien fester Größe als Ringpuffer. Das Budget hängt vom freien Platz ab (Reserve für Datei-Log, Konfiguration und LittleFS-Compaction); das älteste Segment wird gelöscht. Puffer im RAM, Flush ab Schwelle oder spätestens nach 5 min, nur aus der Hauptschleife. Das jüngste Segment wird beim Start auf den letzten gültigen Rahmen gekürzt.
- Jedes Segment beginnt mit der Kanaltabelle (Schlüssel, Name, Einheit, Schwellen, Flags analog/einseitig), weil sich die Kanalbelegung ändern kann.
- Endpunkte: Seite `/chronik`; Datenstrom ab Zeitpunkt (Binärformat, der Browser dekodiert); CSV-Export (`zeit,kanal,schluessel,wert,rohwert,status`, Dateiname `chronik.csv`, gedrosselt auf einmal pro 10 s).
- Seite: Bereichsknöpfe 1 h / 6 h / 24 h (Standard) / Alles, CSV-Knopf, Canvas-Diagramm.
  - Spuren je Kanal; Rohwert-Spuren für Analogkanäle sind anfangs aus.
  - Achsen je Einheit, abwechselnd links und rechts; Lücken über max(120 s, 3×Median) unterbrechen die Linie; Bucketing pro Pixelspalte.
  - Bei genau einer sichtbaren Reihe: Schwellenlinien und ein grünes Band.
  - Mausrad zoomt (Faktor 1,25/0,8, Minimum 5 min), Ziehen verschiebt; Legende mit Min/Mittel/Max im Ausschnitt, Klick blendet ein oder aus; HiDPI.
  - Alle 60 s (bei sichtbarem Tab) inkrementell nachladen.
  - Infotabelle: Segmente x von y, belegt, Aufzeichnung aktiv/aus (Grund), verworfene Rahmen, nicht aufgezeichnete Kanäle (über 16).
- Neu durch E6: Der Hinweis „Dateisystem-Update löscht Chronik“ entfällt, weil es kein FS-Update mehr gibt.

### 2.7 Display (F-07)

SSD1306 128×64. Bildschirme rotieren alle `screen_dur` in dieser Reihenfolge, jeweils nur wenn aktiviert:

1. **Info:** `<mdns>.local` (umbrochen auf bis zu 2 Zeilen), IP, SSID; zu lange Texte enden mit „~“.
2. **Uhr:** HH:MM groß, Datum TT.MM.JJJJ klein. Ohne gültige Zeit wird der Bildschirm **übersprungen**, ohne einen Rotationsschritt zu verbrauchen (Befund B-34). Im 12h-Format „AM/PM“ anzeigen oder die Option umbenennen (Befund B-35).
3. **QR-Code** zu `http://<ip>`: Version 2 bzw. 3, ECC low, Skalierung 2, unten bündig, Titel „QR zu Website:“.
4. **Blumen-Bitmap**.
5. **Fabmobil-Logo**.
6. Je aktivierter und für das Display freigegebener Kanal: Name klein zentriert, Wert mit 1 Nachkommastelle und Einheit groß zentriert, Linien oben und unten.

Weitere Anforderungen:

- Umlaute werden für den Standardfont zu ae/oe/ue/ss, „°“ zu „*“.
- **Boot-/Update-Logbildschirm:** Kopf „<Gerätename> startet:“ bzw. „… Update:“, danach die letzten 6 Statuszeilen (WLAN-Versuche, AP-SSID und IP mit Anleitung „1. Verbinde mit AP / 2. Browser: <ip>“, NTP, Setup abgeschlossen).
- **OTA-Fortschritt** erscheint auf dem Display.
- Ohne Display-Hardware startet das Gerät normal.

### 2.8 LED-Ampel (F-08)

- Modus 0: alle LEDs aus. Der Altcode schaltet beim Wechsel auf 0 nicht ab (Befund B-37).
- Modus 1: zeigt den Status des gerade auf dem Display angezeigten Kanals; bei Nicht-Kanal-Bildschirmen aus. Ohne Display wird Modus 1 nicht angeboten.
- Modus 2: Status des festen Kanals, sekündlich.
- „Nur Rot“: Gelb und Grün schalten alle LEDs aus; nur Rot leuchtet.
- Unbekannt, Fehler oder Warmup: aus. Ein späteres Blinkmuster wäre möglich, ist aber nicht gefordert.

### 2.9 WLAN, AP, mDNS, Zeit (F-09)

- **Drei Slots**, die beim Start der Reihe nach versucht werden, je höchstens 10 s. Ein Slot gilt als konfiguriert, sobald die SSID gesetzt ist. Leere Passwörter für offene Netze sind erlaubt; der Altcode erlaubt das nicht (Befund B-39).
- **Hostname:** vor dem Verbinden setzen und aus dem Gerätenamen ableiten (getestet in `test_mdns_name`): Kleinbuchstaben; ä/ö/ü/ß → ae/oe/ue/ss; andere Zeichen → „-“; keine doppelten bzw. Rand-„-“; höchstens 63 Zeichen; leer → „pflanzensensor“.
- **Kein Netz → AP-Modus:** SSID = Gerätename (≤ 32 Zeichen), Kanal 1, IP 192.168.4.1/24. Der AP bleibt offen und bekommt ein Captive-Portal-DNS (E9). Im AP-Modus wird alle 5 min für 15 s ein Slot im AP+STA-Modus probiert; bei Erfolg wird der AP beendet.
- **Verbindung verloren:** nicht-blockierend Slot für Slot neu versuchen (10 s je Versuch, rotierend), Prüfung alle 30 s.
- **WLAN-Einstellungen ändern:** Der aktive Slot ist nicht editierbar. Neu: Nach dem Speichern wird ein Reconnect angeboten bzw. passiert beim nächsten Verlust.
- **Statische IP** optional als Build-Profiloption (wie im Altcode).
- **mDNS:** `<hostname>.local` mit Dienst `_http._tcp` auf Port 80; neu ansagen, wenn sich Name oder IP ändert.
- **NTP:** pool.ntp.org, Zeitzone Europe/Berlin (`CET-1CEST,M3.5.0/2,M10.5.0/3`), Resync stündlich, ohne gültige Zeit jede Minute. Nach dem Sync die Systemzeit (`settimeofday`) setzen. Im Neubau `configTime()` bzw. SNTP des Cores statt NTPClient, und es läuft **nie blockierend**; der Altcode wartet beim Start bis zu 10 s.

### 2.10 Logging (F-10)

- Level DEBUG < INFO < WARNING < ERROR. Ausgabe: Serial, optional Datei, optional Live-Stream.
- Format `YYYY-MM-DD HH:MM:SS` (bzw. `<sek>s` ohne Uhr), Kürzel `.D.`/`:I:`/`!W!`/`#E#`, `[Modul]`, Text.
- Debug-Schalter je Bereich (RAM, Messzyklus, Sensor, Display, WebSocket, Mail) aktivieren zusätzliche Debug-Ausgaben dieses Bereichs.
- **Datei-Log** `/log.txt`: RAM-Puffer (höchstens 1 KB; Flush ab 512 B oder nach 5 s, nur aus der Hauptschleife). Bei vollem Puffer werden Zeilen verworfen und gezählt („--- n Logzeilen verworfen ---“). Rotation bei `MAX_LOG_FILE_SIZE = 16000`: neuere Hälfte behalten. Download möglich. Bei Boot-Schleife automatisch aus.
- **Live-Log im Browser** (Seite `/logs`): Status der Verbindung, Auto-Scroll (schaltet sich ab, wenn man hochscrollt, und wieder an, wenn man nach unten scrollt), höchstens 100 Einträge, Log-Level-Knöpfe, Debug-Schalter. Reconnect bis zu 5-mal mit 3 s × Versuch Abstand. Der Altcode löscht die Ansicht jede Minute per `cleanup`-Nachricht; das entfällt (Befund B-55).
- Die Log-Ausgabe ist nicht-blockierend: Live-Senden passiert aus einem Ringpuffer in der Hauptschleife, nicht synchron im Log-Aufruf (Befund B-46).

### 2.11 Mailbenachrichtigungen (F-11)

Altcode `mail/*`, `utils/mail_scheduler.h`, `utils/mail_template.h`; getestet.

- **Arten:** Startmeldung, Warnung, Lebenszeichen, Testmail (inhaltlich ein Lebenszeichen).
- **Einstellungen:**

  | Einstellung | Standard |
  |---|---|
  | an/aus | aus |
  | Mailbot-URL | `http://pflanzensensormailbot.fabmobil.org:45480` |
  | Geräte-ID | – |
  | Schlüssel (32 Byte Base64; wird nie ausgegeben, leer lassen = behalten) | – |
  | Empfänger | – |
  | Auswahl überwachter Kanäle (leer = alle) | leer |
  | Warnen ab | gelb |
  | höchstens eine Warnung alle n h (1..720) | 4 |
  | Startmeldung an/aus | aus |
  | Lebenszeichen an/aus | aus |
  | Lebenszeichen alle n h (1..720) | 24 |

  `last_warning` und `last_alive` (Epoch) werden persistiert und überleben einen Neustart.
- **Scheduler-Regeln:**
  - nichts ohne „an“ und ohne gültige Uhr
  - Startmeldung zuerst: sobald alle aktiven Sensoren einmal gemessen haben oder 300 s vergangen sind
  - Warnung: irgendein überwachter Kanal ≥ Schwelle, frühestens 180 s nach dem Start, frühestens `warn_h` nach der letzten Warnung
  - Lebenszeichen: `alive_h` nach dem letzten bzw. nach dem Start
  - Fehlschlag: Warnung bzw. Lebenszeichen gelten trotzdem als gesendet, damit kein Dauerfeuer entsteht; eine fehlgeschlagene Startmeldung wird übersprungen; eine Testmail wird bei vorübergehendem Fehler bis zu 120 s im 5-s-Takt wiederholt
  - Startzeit nachtragen, wenn die Uhr erst später gültig wird
- **Vorlagen:** Betreff und Text je Art plus ein gemeinsames CSS.
  - Standardtexte siehe `mail_vorlagen_standard.h`; sie werden übernommen.
  - Mini-Markdown: `# ` → `<h1>`, `**x**` → `<strong>`, `[t](url)` nur für http/https/mailto, sonst `<p>`, Leerzeilen weg.
  - Platzhalter `{geraet} {name} {ip} {ssid} {neustarts} {laufzeit} {datum} {uhrzeit}` (HTML-maskiert), `{{` → `{`; unbekannte bleiben stehen.
  - Blockplatzhalter allein in einer Zeile: `{messwerte}` erzeugt eine Tabelle aller aktiven Kanäle „<Ampel-Emoji> Name | Wert(1 NK)+Einheit oder --“; `{auffaellige}` nur überwachte und auffällige Kanäle, sonst „✅ Jetzt ist wieder alles im grünen Bereich!“.
  - Grenzen: Betreff ≤ 120 B, Text ≤ 1600 B, CSS ≤ 1200 B ohne `<>`, Zeile ≤ 256 B, gerenderte Mail ≤ 2000 B.
  - Dem Betreff wird „[Gerätename] “ vorangestellt; der Altcode macht das über das Feld `name` im Mailbot (siehe 3.3).
- **Vorlagen-Editor** (Seite): Tabs Startmeldung / Warnung / Lebenszeichen / Design; Betreff- und Textfeld; Byte-Zähler (gelb über 90 %, rot über 100 %); Emoji-Palette (36 Emojis aus dem Altcode); Platzhalter-Knöpfe; Prüfhinweise; Live-Vorschau im sandboxed iframe mit Beispielwerten; Speichern; „Auf Standard zurücksetzen“.
  - Die Template-Engine existiert heute doppelt (C++ und JS). Im Neubau gibt es eine C++-Implementierung plus die JS-Vorschau; **gemeinsame Testvektoren** (JSON-Datei in `test/vectors/`) prüfen beide.
- **Seite E-Mail:** Felder wie oben, „Speichern“, „Speichern und Testmail“, „Nur Testmail“; Statusanzeige „Letzter Versand: … (n erfolgreich)“ mit Polling, solange ein Test läuft; Hinweistexte zum Mailbot und zur AES-Verschlüsselung.

### 2.12 Firmware-Update (OTA) (F-12)

Neu durch E6: nur noch `firmware.bin`.

- **Seite Update:** Version und Build-Datum; Hinweise (Download unter https://github.com/Fabmobil/Pflanzensensor/releases; Einstellungen bleiben erhalten; Strom nicht trennen); Dateiauswahl `.bin`; MD5-Feld, falls „Prüfsumme verlangen“ aktiv ist; Fortschrittsbalken; Status.
- **Ablauf:** Upload direkt im Normalbetrieb mit echtem Fortschritt (XHR `upload.onprogress`).
  - Vor dem Upload: Sensoren pausieren, WebSocket schließen, Chronik und Logs flushen.
  - Der Minimal-/Update-Modus mit Neustart des Altcodes entfällt, wenn der Heap-Bedarf reicht. Das ist zu messen; ansonsten bleibt ein schlanker Update-Modus als Fallback (O3).
  - Danach Neustart und `DeviceWait` bis zum Wiedererscheinen (Erkennung über gesunkene Uptime).
  - Bei Fehler bleibt die alte Firmware aktiv.
- **Prüfungen:** Größe ≤ freier Sketch-Bereich, `Update.setMD5` bei Angabe, Magic-Byte bzw. Header des Images, Leerlauf-Timeout 60 s ohne Datenpaket.
- Erfolgreiches Update: JSON `{success:true, needsReboot:true}`, dann Neustart.

### 2.13 Administration (F-13)

- **Einstellungen** (`/admin`):
  - Systemeinstellungen (Gerätename, Prüfsumme bei OTA, Admin-Passwort mit Wiederholung)
  - Systemaktionen (Werksreset mit Bestätigung und Warnung, Neustart, Log herunterladen, Konfiguration herunterladen/hochladen)
  - WLAN-Slots
  - Systeminformation (freier Heap, Fragmentierung, größter Block, Laufzeit, SSID, RSSI, IP, MAC, Dateisystem gesamt/belegt/frei, Log-Datei-Größe in % von max.)
  - Log-Einstellungen (Datei-Log, Debug-Schalter, Level)
  - Neustart und Reset laufen über `DeviceWait` mit Overlay (90 s bzw. 120 s; Fehlertext mit Hinweis auf Display-IP und den AP unter 192.168.4.1; Knöpfe „Erneut prüfen“ und „Trotzdem neu laden“).
- **Sensoren** (`/admin/sensors`):
  - Karte „Gesicht der Blume“ (Auswahl Kanal)
  - Karte LED-Ampel (Modus, Kanal bei Modus 2, Nur Rot)
  - je Sensor eine Karte:
    - **„aktiviert“-Schalter** (neu; im Altcode fehlt er; Befund B-11)
    - Intervall in Sekunden
    - Knopf „Messen“
    - Fehlerzähler und Status
  - je Kanal:
    - Name; aktiviert
    - letzter Wert
    - beobachtetes Min/Max mit „Zurücksetzen“
    - Schwellen als 4 Zahlenfelder und Schieberegler (Farbverlauf rot|gelb|grün|gelb|rot, 4 ziehbare Griffe, Marker für aktuellen Wert und beobachtetes Min/Max). Gleiche Werte und einseitige Kanäle muss der Regler darstellen können (Befund B-57); der Skalenbereich richtet sich nach der Einheit und nicht nach Rohgrenzen (Befund B-58).
    - bei Analog: „Skala invertieren“ (muss wirken; Befund B-56), Roh-Berechnungsgrenzen min/max (bei Autokal gesperrt), letzter Rohwert, Autokalibrierung (mit Wiki-Link https://github.com/Fabmobil/Pflanzensensor/wiki/automatische-Kalibrierung) und Halbwertszeit, beobachtete Roh-Extrema mit „Zurücksetzen“
    - bei HX711 (neu): „Tarieren“
  - Änderungen werden einzeln automatisch gespeichert (Debounce 1 s je Feld, **pro Feld**, nicht „nur das letzte“; Befund B-50), mit Toast.
  - Live-Aktualisierung der Werte alle 5 s; die Karte blitzt bei einem neuen Wert auf.
- **Display** (`/admin/display`): Dauer, Uhrformat, Bildschirm-Schalter, Kanäle für das Display (aus der Konfiguration, nicht aus Laufzeitdaten; Befund B-36).
- **E-Mail** und **Mailtexte**: siehe F-11.
- **Update**: siehe F-12.
- **Logs** (`/logs`): siehe F-10.

### 2.14 Metriken und Diagnose (F-14)

- `GET /metrics` im Prometheus-Textformat (Namen kompatibel, siehe 3.2).
- `GET /status` mit Uptime, Version und Update-Status; wird von `DeviceWait` benutzt. Format im Neubau frei, aber `uptime` (Sekunden) und `version` müssen enthalten sein.
- Speicherüberwachung: freier Heap, größter Block, Fragmentierung, Minimum seit dem Start. Bei kritischem Heap Warnung ins Log. Ein „Notfall-Cleanup“ wie im Altcode (Sensoren löschen!) gibt es nicht (Befund B-41); stattdessen sind die Budgets fest (5.3).

---

## 3. Externe Schnittstellen, die kompatibel bleiben (E8)

### 3.1 `GET /getLatestValues` (öffentlich)

Das Feldformat bleibt erhalten. Fehler des Altcodes werden behoben; die Behebung ist kompatibel, weil die Ausgabe dadurch erst gültiges JSON wird.

```json
{
  "currentTime": 123456,              // millis() des Geräts
  "deviceName": "…",                  // JSON-escaped (Altcode: unescaped)
  "flowerStatusSensor": "ANALOG_1",
  "ip": "192.168.1.23",
  "sensors": {
    "<SENSORID>_<idx>": {
      "value": 42.13 | null,          // 2 Nachkommastellen
      "unit": "%",
      "lastMeasurement": 120345,      // millis() der letzten erfolgreichen Messung
      "measurementInterval": 60000,   // ms
      "status": "green|yellow|red|error|warmup|unknown",
      "absoluteMin": 12.3 | null,     // Altcode: "inf" -> ungültiges JSON; neu: null
      "absoluteMax": 55.1 | null,
      // nur ANALOG:
      "raw": 512,
      "absoluteRawMin": 300, "absoluteRawMax": 700,
      "calibrationMode": false,
      "minmax": { "min": 250.0, "max": 750.0 }
    }
  },
  "system": {
    "freeHeap": 18000, "heapFragmentation": 12, "rebootCount": 7,
    "version": "3.0.0", "buildDate": "Nov  4 2026", "processedSensors": 2
  }
}
```

Weitere Regeln:

- Kanäle ohne bisher erfolgreiche Messung fehlen; das ist Altverhalten und bleibt so.
- Kanäle mit leerem Feldnamen fehlen im Altcode; neu sind sie enthalten.
- Bei nicht initialisiertem Manager: `"sensors":{}, "error":"…"`.
- Die Antwort wird mit einem Streaming-Writer und festen Puffern erzeugt, ohne `String`-Akkumulation.

### 3.2 Prometheus `GET /metrics`

Namen und Labels sind kompatibel. Die Werte sind Gauges, wie im Altcode:

| Metrik | Labels | Inhalt |
|---|---|---|
| `pflanzensensor_memory_free_bytes` | `device, version, platform, board` | freier Heap |
| `pflanzensensor_memory_max_block_bytes` | dto. | größter Block |
| `pflanzensensor_memory_fragmentation_percent` | dto. | Fragmentierung |
| `pflanzensensor_uptime_seconds` | dto. | Laufzeit |
| `pflanzensensor_wifi_rssi_dbm` | dto. | RSSI |
| `pflanzensensor_wifi_connected` | dto. | 0/1 |
| `pflanzensensor_reboot_count_total` | dto. | Neustartzähler (Gauge, Name bleibt) |
| `pflanzensensor_sensor_value` | `device, sensor_id, field, unit` | je Kanal mit gültigem Wert (ppm == 0 übersprungen) |

Korrekturen:

- `platform` und `board` kommen aus dem Build (ESP8266/ESP32, Boardname) statt fest „ESP8266“/„nodemcuv2“.
- Label-Werte werden nach Prometheus-Regeln escaped.
- Die nie befüllten `pflanzenserver_requests_total` und `pflanzenserver_active_connections` (Tippfehler, ungültige Syntax) **entfallen** (Befund B-60). Falls gewünscht, kommen sie korrekt neu als `pflanzensensor_http_requests_total{handler,code}`.
- Ausgabe gestreamt, nicht als `vector<String>` plus Gesamtstring.

### 3.3 Mailbot-Protokoll (Server extern, bitgenau)

```
POST <mail_url>/send.php        (HTTP, kein TLS)
Content-Type: application/json
X-Device-Mac: <WiFi-MAC "AA:BB:CC:DD:EE:FF">

{"device_id":"<id>","ts":<unix>,"nonce":"<b64 12B>","ciphertext":"<b64>","tag":"<b64 16B>"}
```

- Klartext vor der Verschlüsselung: JSON `{"to":"<empfänger>","subject":"<betreff>","body":"<html>","name":"<gerätename>"}`
- AES-256-GCM, Schlüssel = Base64-Dekodierung des konfigurierten Schlüssels (genau 32 Byte), Nonce 12 B aus dem Hardware-RNG, **AAD = `device_id` + dezimal(`ts`)**, Tag 16 B
- Antwort 200 = Erfolg; sonst JSON `{"error":"…"}` auswerten; HTTP-Status ≤ 0 gilt als vorübergehend
- Grenzen: Betreff ≤ 120 B, Body ≤ 2000 B
- `ts` = aktuelle Epoch (nur mit gültiger Uhr senden)

Neu für RAM (E4): Der Klartext-JSON-Puffer wird **in place** verschlüsselt (ein Puffer ≤ ~2,3 KB). Base64 und Umschlag werden direkt in den TCP-Stream geschrieben (Content-Length vorher berechnen), statt fünf `String`-Kopien zu halten (Altcode-Spitze ~10 KB; Befund B-63). Krypto über eine Plattformabstraktion: ESP8266 BearSSL `br_gcm` mit `br_aes_ct_ctr` und `os_get_random`, ESP32 mbedTLS `mbedtls_gcm_*` mit `esp_fill_random`. Testvektor gegen den Altcode erzeugen und als Unit-Test festhalten.

---

## 4. Befunde im Altcode

Diese Liste dient zwei Zwecken. Erstens zeigt sie, wo der Altcode **nicht** als Verhaltensreferenz taugt: Diese Fehler werden nicht nachgebaut. Zweitens begründet sie die Architekturentscheidungen in Abschnitt 5. Schwere: **K** = kritisch/funktional falsch, **M** = mittel, **G** = gering/kosmetisch.

### 4.1 Fehler und Fehlverhalten

| ID | S | Ort (Altcode) | Befund |
|---|---|---|---|
| B-01 | M | `main_init.cpp` `checkBootLoop` | Vergleicht `millis()` zweier Boots (beim Start immer ~1 s). Jeder Neustart innerhalb von 60 s Laufzeit zählt; schon zwei schnelle manuelle Neustarts schalten die Chronik ab, drei das Datei-Log. |
| B-02 | M | `main_init.cpp` + `config_backup_utils.h` | Beim Restore nach einem FS-Update werden die Preferences **zweimal** wiederhergestellt (`restoreFromFlash` und danach `restoreAllFromFlash`). Entfällt mit E6. |
| B-03 | M | `web_ota_handler.cpp` | Nach einem FS-Update wird `/.restore_from_flash` in das gerade überschriebene, noch gemountete LittleFS geschrieben. Entfällt mit E6. |
| B-04 | M | `manager_config_persistence.cpp` `backupPreferencesToFile` | Der Konfig-Export kennt nur die Sensoren ANALOG und DHT; Mail-Einstellungen und `debug.mail` fehlen; `screen_dur`-Default 5 statt 5000. |
| B-05 | K | Konfig-Export | Enthält WLAN- und Admin-Passwörter im Klartext und landet als Zwischendatei `/prefs_backup.json` im Flash. |
| B-06 | M | `manager_sensor_persistence.cpp` `updateMeasurementInterval` | Scheitert, solange `/config/settings.json` nicht existiert. Die Datei wird nur beim Konfig-Import angelegt, also ist auf einem frischen Gerät das Intervall nicht speicherbar. |
| B-07 | M | `sensor_measurement_state.h` `isDue`, `scheduleNextMeasurement` | `millis() >= nextDueTime` ist nicht überlaufsicher (nach 49,7 Tagen). |
| B-08 | M | `sensor_measurement_cycle_data_processing.cpp` | Ungültige Kanalwerte werden als **0.0** gespeichert und angezeigt; das ist von „0 %“ nicht zu unterscheiden. |
| B-09 | M | `sensors.cpp` `performMeasurementCycle` | `activeValues` = Anzahl gültiger Mittelwerte. Fällt Kanal 0 aus, verschiebt sich die Bedeutung der Indizes, und Process bricht mit „Größenabweichung“ ab. |
| B-10 | K | `sensor_factory.cpp` | Schlägt `init()` beim Start fehl (z.B. DHT steckt nicht), wird der Sensor **nicht** angelegt und verschwindet bis zum Neustart aus Messung und UI. |
| B-11 | K | `manager_sensor.cpp`, `admin_sensor_handler_*` | Zyklusmanager werden nur für beim Start aktivierte Sensoren angelegt. Ein zur Laufzeit aktivierter Sensor misst nie (Logspam „Kein Zyklusmanager“). Die UI zeigt deaktivierte Sensoren gar nicht und hat keinen Aktivieren-Schalter. Gleichzeitig deaktivieren `POST /admin/sensors` und `/admin/sensor_update` einen Sensor, wenn das Feld `enabled_<ID>` fehlt. |
| B-12 | M | `SensorPersistence` | Der Sensor-weite Aktiv-Zustand wird nicht persistiert. |
| B-13 | M | `SensorManager::initialize` | Die Prüfung „dauerhaft fehlerhafter Sensor“ läuft, **bevor** die Persistenz geladen ist, und ist damit wirkungslos. |
| B-14 | M | `SensorPersistence::load` | Überschreibt die komplette `MeasurementConfig` und setzt dabei den Autokal-Float-Zustand auf 0/1023 und `autocalHalfLifeSeconds` auf 0, wenn das Feld fehlt. Wird nach jedem „Min/Max zurücksetzen“ erneut aufgerufen. |
| B-15 | K | Write-Behind-Queue | `lastValue` ändert sich in jedem Zyklus, daher wird **jede Messung jede Kanaldatei neu geschrieben** (laden, ändern, temporär schreiben, löschen, umbenennen). Bei zwei Kanälen im Minutentakt sind das rund 2900 Dateischreibvorgänge pro Tag; der Kommentar „reduziert Flash-Wear“ trifft nicht zu. |
| B-16 | M | `json_file_utils.h` `saveJsonFile` | Zwischen `remove(path)` und `rename(tmp, path)` existiert keine Datei; ein Stromausfall verliert die Einstellung, und `.tmp` wird beim Laden nicht berücksichtigt. |
| B-17 | K | `sensor_mhz19.cpp` | `pulseIn` blockiert pro Sample bis zu ~6 s, pro Zyklus bis ~18 s; Webserver und WLAN stehen in dieser Zeit. |
| B-18 | G | `sensor_ds18b20.cpp` | Löst pro Sensorindex eine Konversion **aller** Sensoren aus (N × 750 ms statt 1 × 750 ms); kein Mittelwert. |
| B-19 | K | `sensor_sds011.cpp` | Schläft nach **jedem einzelnen Sample** (wegen „pending“-Rückgabe) und wacht wieder auf; das Warmup gilt nur einmal nach dem Start. `readResponse` blockiert. |
| B-20 | M | `sensors.h` `usesOneSidedLimits` | Einseitige Bewertung per ID-Präfix: MH-Z19 wird einseitig bewertet, obwohl seine Defaults zweiseitig sind. MH-Z19 `range` ist 2000, `MHZ19_MAX` aber 5000. |
| B-21 | K | `sensor_hx711.cpp`, `sensor_bmp280.cpp` | `init()` setzt `m_initialized` nie. Der Zyklus ruft `init()` 5-mal auf und geht dann in den Fehler; **HX711 und BMP280 messen praktisch nie**. Der HX711 tariert bei jedem Init, damit geht der Nullpunkt einer belasteten Waage verloren. |
| B-22 | K | `sensor_serial_receiver.cpp` | `configureMeasurements()` wird nie aufgerufen (1 Kanal ohne Namen). `measurementInterval` = 0 bedeutet Dauermessung. Baud-, Timeout- und Intervall-Makros werden ignoriert. Liest blockierend bis 4 s pro Sample. |
| B-23 | M | `sensor_dht.cpp` | `init()` meldet immer Erfolg, auch ohne angeschlossenen Sensor. |
| B-24 | G | `sensor_analog.h` | `ANALOG_n_UNIT` wird ignoriert (Einheit fest „%“). Defaults gibt es dreifach: Config-Switch, Konstruktortabelle und `sensor_config.h`. |
| B-25 | M | `admin_sensor_handler_pages/config.cpp` | „inverted“: `newInverted = hasArg(...)` steht innerhalb `if (hasArg(...))`, damit ist der Wert immer `true`. |
| B-26 | M | `admin_sensor_handler_thresholds.cpp` | Erzwingt `yl ≤ gl ≤ gh ≤ yh`, auch bei einseitigen Sensoren. |
| B-27 | M | `admin_sensor_handler_config.cpp` `handleGetSensorConfigJson` | Strings unescaped per `snprintf` in einen 256-B-Puffer; lange Namen ergeben kaputtes JSON. |
| B-28 | M | `sensor_handler.cpp` `/getLatestValues` | `absoluteMin/Max` = ±inf wird als `inf` ausgegeben (ungültiges JSON); `deviceName` unescaped. |
| B-29 | G | `sensor_handler.cpp` | Kanäle mit leerem Feldnamen werden aus `/getLatestValues` stillschweigend weggelassen. |
| B-30 | M | `admin_sensor_handler_ui.cpp` | Schwellen und letzter Messwert werden als `int` gerendert; Nachkommastellen gehen beim nächsten Speichern verloren. |
| B-31 | M | `web_manager_handlers.cpp` | `setConfigValue` für Namespace `debug` und `log/level` **ohne Authentifizierung**. |
| B-32 | K | `web_auth.h` | Fest einkompiliertes Notfall-Adminpasswort, öffentlich im Repository. **Bleibt per Nutzerentscheidung (E9)**; nur als Risiko dokumentiert. |
| B-33 | M | `log_handler.cpp`, `websocket.cpp` | Live-Log-WebSocket (Port 81) ohne Authentifizierung; darüber lässt sich das Log-Level setzen. |
| B-34 | G | `manager_display.cpp` `rotateScreen` | Uhr ohne NTP: Der Rotationsschritt wird verbraucht, der alte Bildschirm bleibt stehen. Der Vergleich auf „Time not synced“ ist tot. |
| B-35 | G | Display | Option „12-Stunden (AM/PM)“ zeigt nie AM/PM. |
| B-36 | M | `admin_display_handler.cpp` | Die Kanalliste wird aus Laufzeit-Messdaten (`activeValues`) statt aus der Konfiguration gebaut und ändert sich je nach Messerfolg. |
| B-37 | M | LED-Ampel | Beim Wechsel auf Modus 0 bleiben die LEDs im letzten Zustand; Modus 1 ohne Display ist wirkungslos. |
| B-38 | G | `utils/wifi.cpp` `checkAPModeRecovery` | `g_activeWiFiSlot = (retrySlot+2)%3` ist falsch, wenn Slots übersprungen wurden. |
| B-39 | M | `utils/wifi.cpp` | Slots ohne Passwort gelten als leer; offene Netze sind unmöglich. |
| B-40 | M | `main_wifi.cpp` | NTP blockiert beim Start bis 10 s, WLAN bis 3 × 10 s; im AP-Modus kein Captive-DNS (Name „CaptivePortal“ irreführend). |
| B-41 | K | `main.cpp` `loop` | Bei Heap < 3000 wird `sensorManager->cleanup()` aufgerufen, also **alle Sensoren gelöscht**; das Gerät misst bis zum Neustart nicht mehr. |
| B-42 | M | `memory_manager.cpp` | `minFreeHeap` wird nur beim Reset gesetzt, nie nachgeführt; `totalHeap` ist fest 81920. |
| B-43 | M | `websocket.cpp` | Sendepuffer 256 B: Logzeilen mit JSON-Hülle über 256 B werden kommentarlos verworfen. Höchstens ein Client. |
| B-44 | M | `logger.cpp` | Meldungen werden auf 128 Zeichen abgeschnitten (`snprintf`). |
| B-45 | M | `log_handler.cpp` | Der Logger-Callback existiert nur, solange der Log-Handler im LRU-Cache ist; nach der Verdrängung gibt es kein Live-Log mehr. |
| B-46 | M | `log_handler.cpp` | Senden über WebSocket **synchron aus jedem Log-Aufruf** (TCP aus beliebigem Kontext). |
| B-47 | M | `web_manager_static.cpp` / `base_handler.h` | `collectHeaders` sammelt nur `Accept-Encoding`; alle Prüfungen auf `X-Requested-With`, `Accept` und `Content-Type` laufen ins Leere. Wirksam ist nur `?ajax=1`. |
| B-48 | G | `components.cpp`, `admin_handler_*` | Gerätename, SSID, Titel und Fehlermeldungen unescaped in HTML-Attributen bzw. Text (XSS über die eigene Konfiguration). |
| B-49 | G | `admin.js` vs. `manager_config_validator.cpp` | Passwortlänge: Client 8..32, Server 3..32. Gerätename: UI ≤ 32, Server ≤ 64. |
| B-50 | M | `admin.js` `initConfigAutoSave` | Speichert nach 1 s Debounce nur das **zuletzt** geänderte Feld; zwei schnell geänderte Felder verlieren das erste. Im Submit-Pfad steht eine undefinierte Variable `actionAttr`. |
| B-51 | M | `admin.js` | Checkbox `debug_mail` auf `/logs` hat kein Mapping und ist wirkungslos. |
| B-52 | G | Startseite | Status-Texte serverseitig „OK/Warnung/Kritisch“, clientseitig „OK/WARNUNG/KRITISCH“. |
| B-53 | M | `admin_handler_wifi.cpp` | WLAN-Passwörter im Klartext im HTML (`value='…'`). |
| B-54 | M | `web_manager_routes.cpp` | Ergebnis des Konfig-Uploads über Flag-**Dateien** im Flash; Antwort mit `delay` + `handleClient`-Hack vor dem Neustart. |
| B-55 | G | `log_handler.cpp` + `logs.js` | Server schickt alle 60 s `cleanup`, der Client leert daraufhin die Logansicht. `heartbeat`/`pong` werden nie gesendet. |
| B-56 | K | `admin_sensors.js` `updateAnalogInverted` | „Skala invertieren“ schreibt in den Preferences-Namespace `s_ANALOG` (Key `m<i>_inv`), den niemand liest; **die Invertierung über die UI ist wirkungslos** und legt ein 16-KB-Verzeichnis an. |
| B-57 | G | `admin_sensors.js` `ThresholdSlider` | Verlangt streng steigende Schwellen; einseitige oder gleiche Werte ergeben keinen Regler. |
| B-58 | G | `admin_sensors.js` `calculateRange` | Für Analogkanäle wird der Skalenbereich aus den Roh-Grenzen (z.B. 250..750) gebildet, die Schwellen sind aber Prozent. |
| B-59 | M | `admin_minimal_handler.h` | Im Update-Modus angelegt, aber `registerRoutes` wird nie aufgerufen; `/admin/reboot` ist im Update-Modus nicht erreichbar. |
| B-60 | G | `web_metrics_handler.cpp` | `incrementRequestCounter` und `setActiveConnections` werden nie aufgerufen; die Zählerlogik würde pro Scrape doppelt addieren; Labelsyntax ungültig; Tippfehler „pflanzenserver“. |
| B-61 | G | `components.cpp`, Metriken, `/getLatestValues`, Mails | `/reboot_count.txt` wird bei jedem Seitenaufruf bzw. Scrape aus dem Flash gelesen. |
| B-62 | M | `web_ota_handler.cpp` | `beginUpdate` (Prüfung der Update-Flags) wird nie aufgerufen; ein Upload ist auch im Normalbetrieb mit vollem Heap-Verbrauch möglich. Der Upload-Fortschritt im Browser ist unecht (springt auf 70 %). |
| B-63 | M | `mail_sender.cpp`, `mail_client.cpp`, `mail_crypto.cpp` | Mailversand hält Rumpf, Klartext-JSON, Kopie, Base64 und Umschlag gleichzeitig: ~10 KB Heap-Spitze. |
| B-64 | G | `main.cpp`, `sensors.h` | `getMeasurementData()` wird mehrfach **per Wert** kopiert (~300 B Stack je Kopie) in Display, Startseite und Admin. |
| B-66 | M | `utils/wifi.cpp`, `mail_crypto.cpp`, `flash_persistence.cpp` | Trotz `#ifdef ESP32` an vielen Stellen baut der Altcode **nicht für ESP32**: `<ESP8266mDNS.h>` wird unbedingt eingebunden, außerdem BearSSL `br_gcm`, `os_get_random`, `Dir`/`openDir`, `ESP.getFreeContStack`. |
| B-65 | M | `resetToDefaults` | Löscht `/config/sensor_*.json`, aber weder `settings.json` noch die Mailvorlagen; der Text im UI verspricht mehr. |

### 4.2 Toter oder wirkungsloser Code (nicht nachbauen)

| Bereich | Elemente |
|---|---|
| Konfiguration | `SensorErrorTracker` (leere Klasse), `SensorConfigData`, `Thresholds`-Struct in Teilen, `ConfigNotifier` ignoriert `updateSensors` (und hat keine relevanten Abonnenten), Legacy-API `setConfigValue(key,value)` neben der Namespace-API, Preferences-Key `log/file_enabled` (doppelt zu `general/file_log`), `DebugConfig` dupliziert `ConfigData`, `getConfigSize` (Konstante 570) |
| Persistenz | `FlashPersistence::clearFlash`, `ConfigBackupUtils::backupConfigFiles`, Altformat-v1-Parser, `#ifndef USE_WEBSERVER` (immer definiert), `updateAnalogMinMaxIntegerNoReload` (Alias), mehrere redundante `update*`-Funktionen |
| Sensorik | `startMeasurement`/`continueMeasurement` fast aller Treiber (die Basisklasse misst über `fetchSample`), `validateMemoryState`/`resetMemoryState` (Paranoia-Prüfungen), `getOrInitThresholds`, `ThresholdDefaults`, `retryDelay`/`maxRetries`, doppelte Fehlerzähler, `MeasurementStateInfo` doppelt in Sensor und Zyklus, `needsInitialization`/`measurementStarted`/`lastAttemptTime`, Multiplexer-Felder `m_switchInProgress`/`SWITCH_*`, `AutoCal_init`/`_from_json`/`_to_json`, `isDeviceConnected`/`sendRequest`/`lastData_` (Serial Receiver), `HX711::processResults` |
| Manager | `Manager`-Basisklasse mit Zuständen, die kaum ausgewertet werden; `g_managerState`, `ManagerStateGuard`, `SystemManagerState`; `ResourceManager::initMinimalSystem`, `doFirmwareUpgrade`, `checkMemoryStatus`, Fehlerzähler (die „kritische Operation“ ist nur ein Flag ohne Schutzwirkung); `LedLights::getStatus` |
| Web | `CSSService` (komplett), `BaseHandler::handleGet/handlePost` (überall „bitte registerRoutes verwenden“), `WebRouter::serveStatic`, `WebManager::removeRoute`, `setupServices`, `setFirmwareUpgradeFlag`, `isCaptivePortalAPActive` (WebManager), `ota_handler.h` (`IOTAHandler`), OTA `writeData`/`endUpdate`/`abortUpdate`/`getStatus`/`checkMemory`/`calculateMD5`/`calculateRequiredSpace`, `LogHandler::_messageQueue`/`_content`/`getLogLevelColor`, `WebSocketService::sendBIN`, `UserRole`/`AuthType` |
| Utils/Netz | `checkPort`, `getWiFiSignalStrength`, `getCurrentWiFiStatus`, `getWiFiConnectionAttemptsInfo` (`g_wifiAttemptsInfo` wird nur geschrieben), `Helper::initializeUpgradeMode`, `Helper::displayWiFiConnectionAttempts`, globale `WiFiClient client`, `showSensorStatus`, `showWebServerStatus`, Logger `MSG_*`-PROGMEM-Texte und `readProgmemString`, `m_useColors`, `DisplayManager::logEnabledSensors` (baut Strings, loggt nichts), Display `switchDisplay`/`clear`, `INFLUXDB_ERROR`, `USE_MAIL` (alter SMTP-Pfad), `HOST_IP` |
| JS | `updateFooterStats` (Selektor existiert nicht), `initNavigation` (`.nav-link` existiert nicht), Monkeypatch `addLogEntry`, `formData` in `admin_display.js`, XHR-Fallbacks in `admin_sensors.js` |

### 4.3 Ungünstige Muster (Ursachen für RAM-Probleme und Instabilität)

1. **`String` überall:** Konfiguration, IDs, Status („green“), Fehlermeldungen in jedem `TypedResult`, Routentabelle, Metriken, JSON-Bau per Verkettung. Das führt zu Heap-Fragmentierung, die der Altcode dann mit Heap-Schwellen in jeder Funktion „überwacht“.
2. **Drei parallele Persistenzwege:** Preferences (eine Datei pro Key in `/nvs/<ns>/<key>`, ein Verzeichnis pro Namespace kostet 16 KB), JSON-Dateien pro Kanal und eine Flash-Sicherung samt Backup-JSON. Feldlisten werden an 4 bis 5 Stellen von Hand gepflegt, und sie laufen auseinander (B-04).
3. **Globale Singletons mit Querverweisen:** `ConfigMgr`, `ResourceMgr`, `MemoryMgr`, `WebManager`, `ChronikStore`, `MailSender`, `logger`, dazu globale `unique_ptr` (`sensorManager`, `displayManager`, `ledTrafficLightManager`) und `extern`-Deklarationen quer durch die Module. Das ist nicht testbar (der native Test braucht Shims), und es entstehen zyklische Includes.
4. **Serverseitiges HTML in C++** (tausende `sendChunk`-Zeilen), jedes Fragment als eigener TCP-Write, Inline-JS in C++-Strings, Logik doppelt in C++ und JS.
5. **Lazy-Handler mit LRU-Cache und dynamischer Routen-(De)Registrierung:** eine Sparmaßnahme, die eigene Fehler erzeugt (B-45, B-59) und durch statische Routen plus zustandslose Handler überflüssig wird.
6. **Blockierende Treiber** im Messpfad (B-17, B-19, B-22) und blockierende Starts (B-40).
7. **„Paranoia“-Prüfungen statt Invarianten:** Bounds-Checks und Heap-Checks auf jeder Ebene, `validateMemoryState`; die Invarianten selbst sind nicht durch Typen gesichert.
8. **Compile-Time-Konfiguration als Makro-Wust** (`ANALOG_1_…` bis `ANALOG_8_…`, Aliase wie `SDS011_PIN_RX`), Defaults an drei Stellen.

### 4.4 Was gut ist und übernommen werden soll

- **Chronik-Konzept** (Segment-Ringpuffer, Budget, Reparatur, Binärformat, Browser-Dekodierung) und die Tests dazu.
- **Mail-Scheduler** und **Template-Engine** als hardwarefreie, getestete Header.
- **Messzyklus** fachlich (nicht-blockierend, exklusive Hardware, Warmup, Backoff, Sofortmessung) und die Testszenarien dazu; technisch wird er neu gebaut (2.4). Außerdem `RequestThrottle`, `MdnsName`, `Crc32`.
- **Disziplin „Flash/TLS nur aus loop()“**, `safeYield`, kurze `CriticalSection` nur um einzelne Flash-Operationen.
- **Native Unit-Tests** (Unity, PlatformIO `native`) und **JS-Tests** (`node --test`, ohne Abhängigkeiten); `check_stack.sh`; CI mit Release-Automatik (bewusst ohne Zugangsdaten gebaut).
- **UI-Design** (Pixel-Look, Blume, Blätter-Animationen, DeviceWait-Overlay).

---

## 5. Zielarchitektur

### 5.1 Grundsätze

1. **Kern ohne Arduino:** Die gesamte Fachlogik (Zustandsmaschinen, Bewertung, Autokalibrierung, Chronikformat, Scheduler, Template-Engine, Konfig-Schema, JSON-Writer) liegt in `core/`, hängt nur an der C++-Standardbibliothek und läuft im `native`-Test ohne Shims.
2. **Ports and Adapters:** Hardware und Plattform (GPIO, ADC, I²C, UART, OneWire, Dateisystem, WLAN, HTTP-Server/-Client, Krypto, Zufall, OTA, RTC-RAM, Uhr) sind schmale Schnittstellen in `hal/`. Implementierungen gibt es in `platform/esp8266`, `platform/esp32` und `platform/native` (Fakes für Tests). Plattform-`#ifdef`s gibt es **nur** in `platform/`.
3. **Keine Heap-Allokation im Dauerbetrieb:** Nach dem Start (`setup()`) allokiert die Firmware nichts mehr; Ausnahmen sind dokumentiert (Abschnitt 5.3). Container und Strings haben feste Größe. `String`, `std::string`, `std::vector`, `std::function`, `std::map` sind in `core/`, `services/` und `web/` **verboten** und werden im CI per `grep` geprüft. Bibliotheken dürfen intern allokieren; das wird in den Budgets erfasst.
4. **Nichts blockiert:** Jede Operation ist ein Schritt mit `Pending` / `Ok` / `Error`. Die längste erlaubte Dauer eines Schritts beträgt 50 ms (Ziel ≤ 20 ms), ausgenommen dokumentierte Flash-Erases. Ein Loop-Zeitmesser loggt Überschreitungen bei Debug-Level.
5. **Explizite Abhängigkeiten statt Singletons:** Der Composition Root (`app/main.cpp`) legt alle Dienste als statische Objekte an und reicht Referenzen per Konstruktor weiter. Es gibt keine `extern`-Globals außer dem Composition Root und keine Singletons mit `getInstance()`.
6. **Eine Quelle der Wahrheit je Datum:** Kanaldefinitionen (Name, Einheit, Grenzen, Defaults) gibt es genau einmal als `constexpr`-Tabelle je Treiber. Das Konfig-Schema gibt es genau einmal (Tabelle aus Schlüssel, Typ, Grenzen, Default, Offset); daraus entstehen Laden, Speichern, Validierung, Export/Import und API automatisch.
7. **Flash-Schreiben nur aus der Hauptschleife**, gebündelt und gedrosselt; atomar über Temp-Datei plus `rename` (LittleFS ersetzt beim `rename` atomar). Keine Unterverzeichnisse, weil jedes Verzeichnis 16 KB kostet.
8. **Fehler sind Werte:** `Result<T>` bzw. `Status` mit `enum class Err : uint8_t`; kein `String` im Fehlerpfad. Klartexte kommen aus einer PROGMEM-Tabelle `errText(Err)`. Keine Exceptions, kein RTTI (`-fno-exceptions -fno-rtti`).
9. **Sprache:** C++17 (gnu++17; auf dem ESP8266-Core 3.x verfügbar). Erlaubt: `constexpr`, `std::array`, `std::optional`, `std::string_view`, `std::variant` (sparsam), Templates für statische Polymorphie, wo es Code spart. Virtuelle Funktionen sind nur an Plattform- und Treibergrenzen erlaubt.

### 5.2 Verzeichnisstruktur

```
firmware/
  platformio.ini                 # envs: esp8266_pcb (Standard), esp32_devkit, native
  boards/
    pcb_v1.h                     # Board-Profil eigenes PCB (constexpr Pins, Features, Kanäle)
    esp32_devkit.h               # Referenzprofil ESP32
    profile_check.h              # static_asserts: Pinkonflikte, Grenzen
  src/
    app/
      main.cpp                   # Composition Root: statische Objekte, setup(), loop()
      scheduler.h/.cpp           # kooperativer Task-Takt, Loop-Zeitmessung
      boot_guard.h/.cpp          # Boot-Schleifen- und Reset-Erkennung (RTC-RAM)
    core/                        # Arduino-frei, nativ getestet
      types.h                    # Instant/Duration (überlaufsicher), FixedString<N>, Result<T>, Err
      json_writer.h              # Streaming-JSON in Sink (escape, float, null für NaN/Inf)
      json_reader.h              # Wrapper um ArduinoJson (Filter) für Import/Konfig-Laden
      config/
        schema.h/.cpp            # Tabelle aller Einstellungen (Key, Typ, Grenzen, Default, Offset)
        device_config.h          # POD-Struct der Gerätekonfiguration
        sensor_config.h          # POD je Sensor/Kanal
      sensors/
        channel.h                # ChannelDef (constexpr), ChannelState, Status, Bewertung
        status.h/.cpp            # evaluate(value, limits, oneSided) -> Status
        scheduler.h/.cpp         # Mess-Scheduler: Aufträge, Ressourcen, Fristen, Backoff (5.6)
        aggregate.h              # Median/Gültigkeit je Kanal
        autocal.h/.cpp           # EMA-Autokalibrierung
        analog_mapping.h         # Rohwert -> Prozent
      chronik/
        format.h                 # Rahmenformat v2 (siehe 5.8)
        budget.h, segments.h     # aus Altcode übernehmen
      mail/
        scheduler.h              # aus Altcode übernehmen (Mail::Scheduler)
        template.h               # aus Altcode übernehmen (MailVorlage)
        envelope.h               # Mailbot-Umschlag (Längen, Base64-Streaming)
      util/
        crc32.h, crc8.h, base64.h, mdns_name.h, throttle.h, ring_buffer.h, event_queue.h
    hal/                         # reine Schnittstellen
      clock.h gpio.h adc.h i2c.h uart.h onewire.h fs.h kv_file.h wifi.h
      http_server.h http_client.h crypto.h rng.h ota.h rtc_mem.h display.h
    platform/
      esp8266/  esp32/  native/   # Implementierungen der hal-Schnittstellen
    drivers/                      # Sensor- und Peripherietreiber auf hal
      sensor_driver.h             # Driver-Schnittstelle (5.6)
      analog_mux.h/.cpp  dht.h/.cpp  ds18b20.h/.cpp  sds011.h/.cpp
      mhz19.h/.cpp  hx711.h/.cpp  bmp280.h/.cpp  serial_receiver.h/.cpp
      ssd1306.h/.cpp              # Displaytreiber (eigene Bildschirm-Renderer, s. DisplayService)
      led_traffic.h/.cpp
    services/
      config_service       # laden, validieren, ändern, gedrosselt speichern, Export/Import, Werksreset
      sensor_service       # Scheduler + Konfig + StateStore, Ereignis "Messung fertig", Sofortmessung
      state_store          # Laufzeitzustand der Kanäle (letzter Wert, Extrema, Autocal), gebündelt speichern
      chronik_service      # Recorder + Segmentspeicher
      display_service      # Rotation, Bildschirme, Bootlog
      led_service
      net_service          # WLAN-Slots, AP + Captive-DNS, Reconnect, mDNS
      time_service         # SNTP, Zeitzone, "Uhr gültig"
      log_service          # Level, Serial, Datei-Log, Live-Ringpuffer
      mail_service         # Scheduler + Rendering + Versand (Streaming)
      ota_service          # Upload-Schreiben, Prüfungen, Neustart
      metrics_service      # Prometheus-Ausgabe (Streaming)
      system_info          # Heap, FS, Uptime, Neustarts (gecacht)
    web/
      router.h/.cpp        # statische Routentabelle (constexpr), Auth-Flag je Route
      auth.h/.cpp          # Basic-Auth, Passwort-Hash, Rate-Limit
      api_*.cpp            # Handler je Bereich (zustandslos, schreiben direkt in Response-Sink)
      assets.h             # generiert: gzip-Assets in PROGMEM + ETag + Content-Type
  web/                     # Quellen der Weboberfläche (statisch, ohne Build-Toolchain)
    index.html chronik.html logs.html admin.html sensors.html display.html mail.html
    mail-templates.html update.html
    js/  api.js ui.js footer.js devicewait.js start.js chronik.js logs.js admin.js
         sensors.js display.js mail.js mail-templates.js update.js template-engine.js
    css/ img/              # aus Altcode übernehmen
  tools/
    embed_assets.py        # gzip -9, ETag (sha1 kurz), erzeugt src/web/assets.h (pre-build)
    gen_md5.py             # .md5 neben firmware.bin
    check_stack.sh, check_forbidden.sh (String/vector/new im Kern)
  test/
    native/…               # Unity-Tests für core/ und services/ (mit platform/native)
    js/…                   # node --test
    vectors/               # gemeinsame Testvektoren C++/JS (Mail-Templates, Chronikrahmen, Mailbot-Krypto)
```

Die alte Struktur (`Pflanzensensor/src`, `Pflanzensensor/data`) wird nach dem Umbau entfernt. Bis dahin entsteht der Neubau parallel in `firmware/`, damit der Altcode als Referenz verfügbar bleibt.

### 5.3 Speicher- und Laufzeitbudgets (ESP8266)

Ziel im Normalbetrieb: **≥ 20 KB freier Heap, größter Block ≥ 10 KB** bei offener Weboberfläche. Der Altcode lag bei ca. 15 bis 17 KB frei.

| Bereich | Budget | Umsetzung |
|---|---|---|
| Gerätekonfiguration (RAM) | ≤ 1 KB | POD mit `FixedString` (Name 33, SSID 33, Passwort 65 ×3, URLs 97, …) |
| Sensor- und Kanalkonfiguration | ≤ 1,5 KB | 16 Kanäle × ~80 B + 8 Sensoren × ~16 B |
| Kanalzustand | ≤ 0,8 KB | 16 × ~48 B |
| Mess-Scheduler (Aufträge + Sample-Puffer) | ≤ 1,7 KB | statisch aus dem Board-Profil; Standardprofil (Analog 2 + DHT 2) ~150 B |
| Chronik-RAM-Puffer | 256 B | wie Altcode |
| Log: Datei-Puffer | 1 KB | |
| Log: Live-Ringpuffer | 2 KB | ersetzt WebSocket (5.10) |
| HTTP-Antwortpuffer | 512 B | Sink über `sendContent` |
| HTTP-Request (Bibliothek) | ~2 KB | ESP8266WebServer, 1 Client |
| Display-Framebuffer | 1 KB | eigener Treiber oder Adafruit_SSD1306 (allokiert beim Start) |
| Mailversand (temporär) | ≤ 3 KB | ein Puffer für den Klartext, in-place verschlüsselt; Rest gestreamt |
| OTA (temporär) | Bibliothek | Sensoren pausiert während des Uploads |
| JSON-Import (temporär) | ≤ 4 KB | nur beim Konfig-Import und beim Start (ArduinoJson mit Filter) |

Weitere Vorgaben:

- **Stack:** Jede Funktion ≤ 512 B Rahmen (Prüfung mit `check_stack.sh`, Grenzwert 512); Messpuffer nicht auf dem Stack.
- **Statische Allokation:** Alle Dienste und Treiber sind statische Objekte im Composition Root, sodass ihre Größe im Linker-Map sichtbar ist. Der Build gibt eine Übersicht aus (`.bss`/`.data` je Objektdatei).
- **Dokumentierte Allokationen:** WLAN, TCP, ArduinoJson beim Import, HTTP-Client beim Mailversand, Display-Lib beim Start. Alles andere ist ein Fehler.

### 5.4 Board-Profil und Feature-Auswahl

Die Compile-Zeit-Konfiguration wird zu **einem** `constexpr`-Profil pro Board. Zugangsdaten stehen dort nicht; sie werden über die Weboberfläche bzw. den AP eingerichtet. Optional kommt eine nicht versionierte `secrets.h` für Entwickler hinzu, die der Release-Build ablehnt.

```cpp
// boards/pcb_v1.h  (Skizze)
namespace board {
constexpr Platform kPlatform = Platform::Esp8266;
constexpr const char kName[] = "pcb_v1";

struct Pins {
  static constexpr int8_t adc = A0;
  static constexpr int8_t muxA = 15, muxB = 2, muxC = 16;
  static constexpr int8_t dht = 0;
  static constexpr int8_t ledGreen = 14, ledYellow = 12, ledRed = 13;
  static constexpr int8_t sda = 4, scl = 5;
};

constexpr Features kFeatures{
  .analog = true, .analogChannels = 2, .multiplexer = true,
  .dht = true, .dhtType = DhtType::Dht11,
  .ds18b20 = false, .sds011 = false, .mhz19 = false, .hx711 = false,
  .bmp280 = false, .serialReceiver = false,
  .display = true, .ledTraffic = true,
};

constexpr DeviceDefaults kDefaults{ /* Gerätename, Intervalle, Mail-URL, AP-Basisname … */ };
}  // namespace board
```

- Treiber werden über `if constexpr (board::kFeatures.x)` bzw. Template-Listen eingebunden. Nicht genutzter Code fällt per `--gc-sections` heraus. Bibliotheks-Includes für optionale Sensoren stehen in den Treiber-`.cpp`, die nur bei aktivem Feature gebaut werden (`build_src_filter` je Env, generiert aus dem Profil, oder leere Übersetzungseinheiten über `#if`).
- `profile_check.h` enthält `static_assert`s gegen doppelt belegte Pins und gegen Strapping-Pins mit unzulässiger Nutzung, außerdem `analogChannels ≤ 8` und `analogChannels > 1 ⇒ multiplexer`.

### 5.5 Laufzeitmodell

```cpp
// app/scheduler.h (Skizze)
struct Task { void (*fn)(Instant now); Duration period; Instant next; const char* name; };
// Feste Tabelle im Composition Root; loop() ruft Scheduler::run(now) auf.
```

| Task | Periode | Bemerkung |
|---|---|---|
| `web.handle` | jede Schleife | genau eine Anfrage pro Schleife |
| `net.tick` | 100 ms | DNS (Captive), mDNS-Update, Reconnect-FSM |
| `sensors.tick` | 100 ms | `MeasurementScheduler::tick`; ersetzt den 1-s-Takt und den Sonderfall „forced“ des Altcodes |
| `led.tick` | 1 s | |
| `display.tick` | 100 ms | rotiert nach `screen_dur` |
| `time.tick` | 1 s | Status SNTP; der Resync macht der Core selbst |
| `mail.tick` | 30 s / 5 s | |
| `persist.tick` | 1 s | **einziger Flash-Schreibpfad**: Konfig (Debounce 2 s), Zustand (alle 15 min oder bei Bedarf), Chronik-Flush, Datei-Log-Flush, Reboot-Zähler |
| `system.tick` | 30 s | Heap-Statistik, Minimum nachführen, Warnungen |

**Ereignisse:** Eine `EventQueue<16>` mit `struct Event { EventType type; uint8_t a, b; uint32_t v; }` entkoppelt die Module. Beispiele: `MeasurementDone{sensor}`, `ConfigChanged{section}`, `ClockValid`, `WifiUp`, `WifiDown`, `RebootRequested{reason}`, `FactoryResetRequested`, `OtaStarted`. Sie werden in der Hauptschleife synchron an eine feste Abonnentenliste verteilt. Handler senden ihre Antwort **vor** einem `RebootRequested`; der Neustart erfolgt in `persist.tick`, nachdem alles geschrieben ist (ersetzt die `delay`/`handleClient`-Hacks; Befund B-54).

### 5.6 Sensorik

Umsetzung des Messmodells aus 2.4.

```cpp
// drivers/sensor_driver.h (Skizze)
enum class Step : uint8_t { Pending, Ok, Error };

enum Resource : uint8_t { None = 0, Adc = 1, I2c = 2, OneWire = 4, SoftUart = 8 };

struct ChannelDef {                 // constexpr je Treiber
  const char* key;                  // "lufttemperatur" (Feldname)
  const char* name;                 // Standardname
  const char* unit;
  float validMin, validMax;
  Limits defaults;                  // yl, gl, gh, yh
  bool oneSided;
  bool hasRaw;                      // Analog: Rohwert führen
};

struct SensorInfo {
  const char* id;                   // "ANALOG", "DHT", …
  span<const ChannelDef> channels;
  uint8_t resources;                // Resource-Maske, nur während einer Runde belegt
  Duration defaultInterval;
  Duration startWarmup;             // einmalig nach dem Start (MH-Z19)
  Duration prepareWarmup;           // pro Messung nach wake() (SDS011)
  Duration roundDelay;              // Abstand zwischen Runden
  Duration roundTimeout;            // Höchstdauer einer Runde
  uint8_t rounds;                   // Standard 3
};

struct Reading { float value = NAN; int16_t raw = -1; };   // NaN = ungültig

class SensorDriver {
 public:
  virtual const SensorInfo& info() const = 0;
  virtual uint8_t channelCount() const = 0;          // z.B. gefundene DS18B20, konfigurierte Analogkanäle
  virtual Step begin() = 0;                          // idempotent, mehrschrittig möglich
  virtual Step wake() { return Step::Ok; }           // SDS011: Work-Mode
  virtual Step startRound() { return Step::Ok; }     // z.B. DS18B20 requestTemperatures, SDS011 Query
  virtual Step pollRound(span<Reading> out) = 0;     // Pending bis alle Kanäle der Runde gelesen sind
  virtual void sleep() {}                            // SDS011 Sleep, HX711 power_down
 protected:
  ~SensorDriver() = default;
};
```

**`core/sensors/scheduler`** (Arduino-frei, nativ getestet):

```cpp
// Skizze
template <size_t MaxSensors>
class MeasurementScheduler {
 public:
  void add(SensorDriver&, SensorRuntime&);   // Runtime: Konfig-Referenz, Fälligkeit, Health, Auftrag
  MeasureNowResult measureNow(uint8_t sensor, Instant now);   // Accepted | Busy | NotReady | Disabled
  void setEnabled(uint8_t sensor, bool, Instant now);
  void tick(Instant now);                    // alle 100 ms aus der Hauptschleife
 private:
  uint8_t busyResources_ = 0;                // vom Scheduler belegte Ressourcen
  // je Sensor ein Job { Phase phase; Instant deadline, phaseSince; uint8_t round; Reading samples[rounds][channels]; }
};
```

`tick()`-Ablauf:

1. **Fristen prüfen:** Abgelaufene Aufträge abbrechen (`sleep()`, Ressourcen frei, Fehlschlag).
2. **Laufende Aufträge weiterschalten:**
   - *Prepare:* `begin()`/`wake()` bis `Ok`, danach Warmup-Ende abwarten.
   - *Sample:* Ist die Runde fällig und sind die Ressourcen frei, Ressourcen belegen, `startRound()`, `pollRound()` bis `Ok`, Ressourcen frei, Rundenabstand abwarten.
   - *Finish:* Median je Kanal, Gültigkeit, `sleep()`, Ergebnis an `SensorService`.
3. **Neue Aufträge starten:** Fällige Sensoren in Reihenfolge Priorität (Sofortmessung) vor Fälligkeit; mehrere gleichzeitig erlaubt, Ressourcenkonflikte lösen sich auf Rundenebene.

Der Speicher für Samples ist statisch: `MaxSensors × rounds × channels × sizeof(Reading)`. Bei 8 Sensoren, 3 Runden und 8 Kanälen sind das 1,5 KB; für das Standardprofil (Analog 2 + DHT 2) entsprechend weniger. Die Größe ergibt sich aus dem Board-Profil zur Compile-Zeit.

**Treiber im Neubau:**

- **Analog:** `pollRound()` liest alle konfigurierten Kanäle in einem Aufruf (8 × ~1,1 ms ≈ 9 ms). Die Abbildung Rohwert → Prozent und die Autokalibrierung erledigt `core` im *Finish*, nicht der Treiber; die Autokalibrierung bekommt dafür den **Median-Rohwert** der Messung.
- **DHT:** `pollRound()` macht eine Lesung (~5 ms mit gesperrten Interrupts, akzeptabel) und liefert beide Kanäle.
- **DS18B20:** `startRound()` = `requestTemperatures()` asynchron (`setWaitForConversion(false)`), `pollRound()` wartet auf das Ende der Konversion und liest alle Geräte.
- **SDS011:** `wake()` = Work-Mode, Warmup über `prepareWarmup`; `startRound()` = Query; `pollRound()` sammelt Bytes nicht-blockierend in einen Rahmenpuffer.
- **MH-Z19:** ISR (`attachInterrupt(CHANGE)`, IRAM) misst `th`/`tl` dauerhaft; `pollRound()` liefert den nächsten vollständigen PWM-Zyklus, Timeout 3 s.
- **HX711, BMP280:** je Runde eine Lesung; HX711 mit persistentem Tara-Offset.
- **Serial Receiver:** `startRound()` sendet `GET:<idx>` nacheinander für alle Kanäle; `pollRound()` sammelt Antwortzeilen nicht-blockierend; Timeout aus dem Profil.

**Weitere Punkte:**

- **Kanalzustand:** `ChannelState { float value=NaN; int16_t raw=-1; Status status=Unknown; Instant lastOk; float obsMin=+Inf, obsMax=-Inf; int16_t rawMin=INT16_MAX, rawMax=INT16_MIN; AutoCalState cal; }`. Er liegt im `StateStore` und nicht im Treiber.
- **`SensorService`** verbindet Scheduler, Konfiguration und `StateStore`, setzt Status und Extrema und veröffentlicht `MeasurementDone`. `measureNow()` liefert `Accepted` / `Busy` / `NotReady` / `Disabled`; die API übersetzt das in 200 / 409 / 409 (mit Restzeit) / 409. Die öffentliche Drossel (2 s global, `RequestThrottle`) ergibt 429.
- **Tests** (gegen `FakeDriver` mit steuerbarer Uhr):
  - Analog und DHT messen parallel ohne Konflikt; zwei `Adc`-Sensoren nie gleichzeitig in einer Runde.
  - SDS011-Warmup blockiert andere Sensoren nicht.
  - Frist-Abbruch gibt Ressourcen frei.
  - Sofortmessung während laufendem Auftrag liefert `Busy`, während Start-Warmup `NotReady`.
  - Backoff-Stufen; Erholung nach Erfolg.
  - Ausfall eines Kanals lässt die anderen gültig (B-09).
  - Keine Drift über 1000 Intervalle; `millis()`-Überlauf.
  - Regressionsszenarien aus den Altcode-Tests: Warmup-Endlosschleife (`test_measurement_cycle`), Selbstblockade bei Sofortmessung (`test_limiter`, `test_sensor_preemption`).

### 5.7 Persistenz

Alle Dateien liegen im **Wurzelverzeichnis** von LittleFS (kein Unterverzeichnis; 5.1 Punkt 7):

| Datei | Inhalt | Format | Schreibhäufigkeit |
|---|---|---|---|
| `/device.cfg` | Gerätekonfiguration | JSON (schema-getrieben, Version) | bei Änderung, Debounce 2 s |
| `/sensors.cfg` | Sensor- und Kanalkonfiguration | JSON | bei Änderung, Debounce 2 s |
| `/secrets.cfg` | WLAN-Passwörter, Admin-Hash, Mail-Schlüssel | JSON, getrennt, nie exportiert | bei Änderung |
| `/state.bin` | Kanalzustand (letzter Wert, Extrema, Autocal-Float) | binär, Version + CRC32 | alle 15 min, bei Extrem-Änderung frühestens nach 5 min, vor Neustart/OTA |
| `/mail.tpl` | Mailvorlagen (nur angepasste Abschnitte) | Format `#MV2` aus Altcode | bei Änderung |
| `/boot.cnt` | Neustartzähler | 4 Byte | einmal pro Boot, danach im RAM |
| `/log.txt` | Datei-Log | Text | Flush-Regeln F-10 |
| `/chr_XXXXXXXX.dat` | Chronik-Segmente | binär (5.8) | Flush-Regeln F-06 |

Regeln:

- **Atomar:** in `<name>.tmp` schreiben, schließen, `rename(tmp, name)` (LittleFS ersetzt atomar). Beim Laden: Fehlt `<name>`, aber `<name>.tmp` existiert und ist gültig, dann wird es übernommen.
- **Validierung beim Laden** gegen das Schema; ungültige Werte werden durch den Default ersetzt und als Warnung geloggt, ohne den Rest zu verwerfen.
- **Keine Preferences-Bibliothek** mehr; sie legt ein Verzeichnis je Namespace und eine Datei je Key an.
- **Export:** `/device.cfg` und `/sensors.cfg` plus Vorlagen als ein JSON-Dokument, gestreamt. Import: hochladen in `/import.tmp`, prüfen, übernehmen, Neustart-Ereignis.
- **Werksreset:** `*.cfg`, `state.bin`, `mail.tpl` löschen, Neustart; Chronik und Log bleiben (E10).
- **Flash-Verschleiß:** Ziel ist, dass im Normalbetrieb nur Chronik-Flushes (≤ 1 pro 5 min), Zustand (≤ 1 pro 15 min) und Datei-Log (falls an) schreiben.

### 5.8 Chronik

Konzept und Code aus dem Altcode übernehmen (`chronik_store.cpp`, `chronik_recorder.cpp`, `utils/chronik_*.h`, `chronik.js`). Änderungen:

- **Format v2:** Je Kanal speichert die Tabelle `encoding` (0 = half, 1 = int16 mit `scale`/`offset`, 2 = float32). Ein Sample-Rahmen kodiert den Wert nach dieser Angabe. Standard: half für %, °C, Feuchte; int16 mit scale 0,1 für hPa; float32 für Gewicht und Zählerstände. Magic-Werte ändern (`0xC6A5`/`0xC6A7`), damit alte Segmente sicher verworfen werden; nach E2 ist keine Migration nötig. Encoder und Decoder in C++ und JS werden über gemeinsame Testvektoren geprüft.
- **Kanalindex:** Er wird nicht mehr aus der laufenden Aktivierungsreihenfolge abgeleitet, sondern ist stabil, z.B. die Position in der Konfig-Kanalliste (0..15). Die Tabelle bleibt pro Segment.
- **Budget:** `RESERVE_COMPACT`/`RESERVE_CONFIG` neu berechnen. Es gibt keine Preferences-Verzeichnisse mehr, also wird mehr Platz frei.
- Der Aufnahme-Haken ist das Ereignis `MeasurementDone`; Flush nur in `persist.tick`.

### 5.9 Web-Schicht und API

**Server:** Die Bibliothek `ESP8266WebServer` (ESP32: `WebServer`, gleiche API) liegt hinter `hal/http_server.h`. Eine Anfrage wird pro Schleife bearbeitet. Kein AsyncWebServer, weil er zu viel RAM pro Verbindung braucht und auf dem ESP8266 Stabilitätsrisiken hat.

**Routing:** Statische Tabelle; `onNotFound` dispatcht über die Tabelle. Keine Laufzeitregistrierung, kein Handler-Cache.

```cpp
struct Route { HttpMethod method; const char* path; Access access; void (*handler)(Request&, Response&); };
// Access::Public | Access::Admin.  Pfadmuster ohne Parameter; Parameter als Query/Form.
```

- **Antworten:** `Response` stellt einen gepufferten Sink bereit (512 B, `sendContent` bei vollem Puffer, `Content-Length` wenn bekannt, sonst chunked). `JsonWriter` schreibt direkt in den Sink.
- **Assets:** `assets.h` enthält gzip-Daten in PROGMEM. Ausgeliefert wird mit `Content-Encoding: gzip`, `ETag` und `Cache-Control: max-age=86400` (HTML: `no-cache`), dazu `304` bei passendem `If-None-Match`. Browser ohne gzip werden nicht unterstützt (vertretbar). `collectHeaders` muss `If-None-Match`, `Accept-Encoding`, `Content-Type` und `Authorization` enthalten (Befund B-47).
- **Authentifizierung:** HTTP Basic, Benutzer `admin`.
  - Passwort gespeichert als `SHA-256(salt‖passwort)` mit 16-B-Salt.
  - Fehlversuche: nach 5 Fehlern 30 s Sperre (pro Gerät).
  - Zusätzlich gilt das **fest einkompilierte Notfallpasswort** (E9) wie im Altcode: Benutzer `admin`, Konstante im Code, bei Nutzung höchstens einmal pro Minute eine Warnung ins Log. Die Konstante liegt im Board-unabhängigen Code (`web/auth.cpp`) und wird mit gehasht verglichen, damit sie nicht als Klartext-String im Image steht (Strings-Suche); der Wert selbst bleibt der des Altcodes.

**Routen** (neu: `/api/v1/…`; dazu die kompatiblen Pfade):

| Methode | Pfad | Zugriff | Zweck |
|---|---|---|---|
| GET | `/`, `/chronik`, `/logs`, `/admin`, `/admin/sensors`, `/admin/display`, `/admin/mail`, `/admin/mail/templates`, `/admin/update` | Seiten: Public bzw. Admin (Basic-Auth beim Laden) | statische HTML-Seiten |
| GET | `/css/*`, `/js/*`, `/img/*`, `/favicon.ico` | Public | Assets |
| GET | `/getLatestValues` | Public | **kompatibel** (3.1) |
| GET | `/metrics` | Public | **kompatibel** (3.2) |
| GET | `/status` | Public | `{uptime, version, bootId}` für DeviceWait |
| GET | `/api/v1/system` | Public | Gerätename, Version, Build, IP, SSID, Hostname, RSSI, Uptime, Neustarts, Zeit gültig, Features (Display ja/nein …), Heap-Kennzahlen; für die Fußzeile |
| POST | `/api/v1/measure` | Public | `{sensor}` → 200/404/409/429 (Retry-After) wie Altcode `/measure` |
| GET | `/api/v1/chronik/data?since=<epoch>` | Public | Binärstrom |
| GET | `/api/v1/chronik/info` | Public | Segmente, Belegung, Status, verworfen, übersprungene Kanäle |
| GET | `/api/v1/chronik/export.csv` | Public | CSV, gedrosselt |
| GET | `/api/v1/config` | Admin | gesamte Konfiguration als JSON, Geheimnisse nur als `"set": true/false` |
| PATCH | `/api/v1/config` | Admin | Teil-JSON `{"display":{"screenDuration":10}}`; schema-validiert; Antwort mit Fehlern je Feld; ersetzt `setConfigValue` und alle Einzelrouten |
| PUT | `/api/v1/secrets/admin-password` | Admin | `{password}` |
| PUT | `/api/v1/secrets/wifi/<1-3>` | Admin | `{ssid, password?}` (Passwort leer = behalten; `clear: true` = löschen) |
| PUT | `/api/v1/secrets/mail-key` | Admin | `{key}` |
| GET | `/api/v1/sensors` | Admin | Sensoren und Kanäle: Konfiguration + Zustand + Status (ersetzt `/admin/getSensorConfig`) |
| PATCH | `/api/v1/sensors` | Admin | Teil-JSON je Sensor/Kanal (enabled, interval, name, limits, inverted, rawMin/Max, autocal, halfLife, display) |
| POST | `/api/v1/sensors/<ID>/measure` | Admin | Sofortmessung ohne öffentliche Drossel |
| POST | `/api/v1/sensors/<ID>/<idx>/reset-extrema` | Admin | `{raw: bool}` |
| POST | `/api/v1/sensors/HX711/tare` | Admin | Tarieren |
| GET | `/api/v1/logs?since=<seq>` | Admin | Live-Log aus dem Ringpuffer: `{next, dropped, lines:[{seq, ts, level, module, text}]}` |
| GET | `/api/v1/logs/file` | Admin | Download `/log.txt` |
| POST | `/api/v1/mail/test` | Admin | Testmail anfordern |
| GET | `/api/v1/mail/status` | Admin | `{pending, sent, result, lastAttempt}` |
| GET / PUT / DELETE | `/api/v1/mail/templates/<boot\|warning\|alive\|style>` | Admin | Vorlage lesen, speichern, auf Standard setzen; Validierung wie F-11 |
| GET | `/api/v1/config/export` | Admin | Download (ohne Geheimnisse) |
| POST | `/api/v1/config/import` | Admin | Upload (multipart) → prüfen → Neustart |
| POST | `/api/v1/system/reboot` | Admin | |
| POST | `/api/v1/system/factory-reset` | Admin | |
| POST | `/api/v1/ota` | Admin | Upload `firmware.bin` (multipart, Feld `firmware`, optional `md5`) |

Ab Pfadparametern wie `<ID>` reicht ein einfacher Präfix-Matcher in der Routentabelle (`/api/v1/sensors/` + Rest parsen).

**Fehlerformat:** `{"ok":false,"error":"<code>","message":"<deutscher Klartext>","fields":{"<pfad>":"<code>"}}`. Erfolg: `{"ok":true, …}`.

### 5.10 Live-Log ohne WebSocket

Der WebSocket-Server (Port 81, Bibliothek links2004/WebSockets) entfällt. Stattdessen:

- `LogService` hält einen Ringpuffer (2 KB) mit fortlaufender Sequenznummer.
- Die Seite `/logs` pollt `GET /api/v1/logs?since=<seq>` jede Sekunde (bei sichtbarem Tab). Zu weit zurückliegende Anfragen bekommen `dropped`.
- Vorteile: kein zweiter Port, keine zusätzliche Bibliothek, Basic-Auth gilt automatisch, nichts wird synchron aus dem Log-Aufruf gesendet (B-46), kein 256-B-Limit (B-43), mehrere Betrachter möglich.
- Das Log-Level wird über `PATCH /api/v1/config` (`log.level`) gesetzt.

### 5.11 Logging-API

```cpp
LOGI(Mod::Wifi, "Verbunden mit %s (%d dBm)", ssid, rssi);   // printf-Stil, Format in PROGMEM (PSTR)
```

- Prüfung von Level und Debug-Bereich **vor** dem Formatieren (Makro).
- Zeile ≤ 160 Zeichen (fester Puffer; Überlänge mit „…“ markiert).
- Ausgabe an Serial, den Datei-Puffer und den Live-Ring.
- `Mod` ist ein `enum class` mit Kurznamen in PROGMEM.

### 5.12 Zeit

- `TimeService` nutzt `configTime(TZ, "pool.ntp.org")` (ESP8266 und ESP32 identisch), ohne NTPClient.
- „Uhr gültig“ = `time() ≥ 1600000000`. Das Ereignis `ClockValid` gibt Chronik und Mail frei.
- `Instant`/`Duration` auf `millis()` mit überlaufsicherer Arithmetik (`int32_t(a - b) >= 0`); Unit-Tests mit Wrap-Around.

### 5.13 Netzwerk

`NetService` ist eine Zustandsmaschine:

```
Boot → TrySlot(i) [10 s] → … → StaUp | ApMode
StaUp → (Verlust) Reconnect(rotierend, 10 s je Slot) → StaUp
ApMode → alle 5 min: ApStaProbe(15 s) → StaUp
```

- Nichts blockiert; der Bootbildschirm zeigt die Versuche (Ereignisse).
- AP-Modus mit Captive-DNS (`DNSServer`, alle Namen auf 192.168.4.1); Antworten auf die üblichen Captive-Checks leiten auf `/admin` um.
- mDNS: `MDNS.begin(hostname)` und `addService("http","tcp",80)`, neu bei Namens- oder IP-Wechsel. Auf dem ESP32 die `ESPmDNS`-Variante (der Altcode baut dort nicht; Befund B-66).

### 5.14 Display

Ein eigener schlanker SSD1306-Treiber (1-KB-Framebuffer, I²C), ein Bitmap-Font 5×7 sowie ein großer Ziffernfont, oder alternativ Adafruit_SSD1306/GFX (heutiger Stand). Entscheidung nach Messung von Flash und RAM (O5).

- QR-Code: ricmoo/QRCode als Bibliothek (`lib_deps`) statt der eingebetteten Kopie.
- Die Bildschirm-Renderer sind reine Funktionen `render(ScreenModel, Framebuffer&)`, nativ testbar (Pixel-Hash).
- Zeichenumsetzung Umlaute/° wie F-07; besser ein Font mit diesen Glyphen (dann entfällt die Umsetzung).

### 5.15 Mail

- `MailService` übernimmt `Mail::Scheduler` und `MailVorlage` unverändert (getestet).
- Rendering streamt in einen festen Body-Puffer (2000 B + Kopf). Danach wird der Klartext-JSON in-place in einen Puffer von ≤ ~2,3 KB gebaut und dort verschlüsselt.
- `HttpClient` (über `hal/http_client.h`) schreibt Header und Base64-Teile direkt in den Socket.
- Der Versand läuft nur aus `mail.tick`. Die Testmail-Logik (Wiederholung bei vorübergehendem Fehler, 120 s) bleibt.
- Krypto über `hal/crypto.h` (`aes256gcmSeal(key, nonce, aad, buf, len, tag)`), Implementierungen siehe 3.3.

### 5.16 OTA

- `OtaService::begin(size, md5?)`: Sensoren pausieren, Chronik und Log flushen, `Update.begin`.
- `write(chunk)`, `end()`; Leerlauf-Timeout 60 s.
- Danach das Ereignis `RebootRequested`.
- Kein Update-Modus mit Neustart (O3). Die Admin-Seite zeigt echten Upload-Fortschritt über XHR.

### 5.17 Weboberfläche

- Statische HTML-Seiten mit gemeinsamem Kopf (Wolke) und Fußzeile (wird von `footer.js` aus `/api/v1/system` befüllt).
- **Kein Build-Schritt** außer gzip (wie heute: Vanilla-JS, `node --test`).
- `api.js`: `get(path)`, `patch(path, obj)`, `put`, `post`, einheitliche Fehlerbehandlung (Toast), 401 → Hinweis.
- `ui.js`: Toast, `bindAutosave(element, path)` mit **Debounce pro Feld** (B-50), Bestätigungsdialoge, Formatierer (Bytes, Uptime, Status-Übersetzung, eine einzige Tabelle).
- `devicewait.js` aus dem Altcode übernehmen (getestet); Erkennung über `bootId` statt Uptime-Heuristik möglich.
- `start.js` (Altcode `sensors.js`), `chronik.js` (übernehmen und auf Format v2 anpassen), `template-engine.js` (Altcode `mailvorlagen.js`, gemeinsam mit den C++-Testvektoren).
- `sensors.js` (Admin) wird neu und deutlich schlanker auf Basis von `/api/v1/sensors`. Der Schwellenregler wird übernommen, aber für einseitige und gleiche Werte korrigiert (B-57, B-58).
- Texte bleiben deutsch und im Ton des Altcodes.

### 5.18 Build, Tests, CI

- **PlatformIO-Envs:**
  - `esp8266_pcb`: `espressif8266` aktuell, `board = nodemcuv2`, Linkerscript übernehmen, `-std=gnu++17 -fno-exceptions -fno-rtti -Os`, Warnungen als Fehler für eigenen Code
  - `esp32_devkit`: ESP32 Arduino, Partitionstabelle mit 2× OTA + LittleFS
  - `native`: alle `core/`-Tests und Service-Tests mit `platform/native`
- **Pre-Build:** `tools/embed_assets.py` erzeugt `src/web/assets.h` und bricht ab, wenn die Gesamtgröße über einem Grenzwert liegt.
- **Checks im CI:**
  - Build beider Plattformen
  - native Tests, JS-Tests
  - `check_stack.sh` (Grenzwert 512 B)
  - `check_forbidden.sh` (kein `String`/`std::vector`/`new`/`std::function` in `core/services/web`)
  - Größenbericht (`.bss`, `.data`, freier Heap nach dem Start wird von einem Hardware-Smoke-Test geloggt, optional)
- **Release:** wie heute (Tag aus Version, `firmware.bin` + `.md5`), aber ohne `littlefs.bin`; je ein Artefakt pro Board-Profil.
- **Versionierung:** `VERSION` in einer Datei; die neue Firmware startet bei **3.0.0**. Der pre-commit-Hook zum automatischen Anheben der Patch-Version bleibt (E10), angepasst auf den neuen Pfad.
- **Gemeinsame Testvektoren** (`test/vectors/*.json`) für Mail-Templates, Chronikrahmen, Mailbot-Krypto (Klartext, Key, Nonce, AAD → Chiffre, Tag), Status-Bewertung und Hostname-Ableitung, ausgewertet von C++- **und** JS-Tests.

---

## 6. Arbeitspakete, Reihenfolge, Abnahme

Die Reihenfolge ist so gewählt, dass jedes Paket allein testbar ist und früh auf echter Hardware läuft. Jedes Paket endet mit grünen Tests im CI. Hardware-Pakete brauchen zusätzlich einen dokumentierten Smoke-Test mit seriellem Log auf dem PCB, bei Bedarf vom Nutzer durchgeführt.

| AP | Inhalt | Liefert F-xx | Abnahme |
|---|---|---|---|
| AP-0 | Gerüst: `firmware/`, PlatformIO-Envs (esp8266_pcb, esp32_devkit, native), Board-Profil + `profile_check`, `core/types` (Instant, FixedString, Result), CI mit Build beider Plattformen, `check_forbidden.sh`, `check_stack.sh` | F-01 | Beide Plattformen bauen ein leeres `setup/loop`. Native Tests für Instant-Wrap, FixedString, Result. Ein absichtlich doppelt belegter Pin bricht den Build. |
| AP-1 | Plattformschicht ESP8266/ESP32/native für Clock, GPIO, ADC, FS, RTC-RAM, Rng; `app/scheduler`, `boot_guard`, `LogService` (Serial + Ring + Datei), `persist.tick` | F-02, F-10 | Boot-Schleifen-Test nativ. Hardware: Log auf Serial, Datei-Log rotiert bei 16 KB, Loop-Zeitmessung < 20 ms im Leerlauf. |
| AP-2 | Konfig-Schema, `ConfigService` (Laden/Validieren/Speichern atomar, Debounce, Werksreset, Export/Import, Secrets), JSON-Writer/Reader, **Import des alten `config.json`** | F-03 | Native Tests: Roundtrip, ungültige Werte → Default, `.tmp`-Wiederherstellung, Export ohne Geheimnisse; Import alter Dateien mit Testdateien aus dem Altcode (neues Format mit `measurements`-Array und Altstruktur ohne). |
| AP-3 | `core/sensors` (Status, Autocal, Analog-Mapping, Aggregation, **MeasurementScheduler**), `StateStore`, `SensorService` mit `FakeDriver` | F-04 | Alle Testfälle aus 5.6 grün, darunter die Regressionsszenarien der Altcode-Tests; Statusregeln aus `test_messzustand`; ungültiger Wert ≠ 0 (B-08), Sensor zur Laufzeit aktivieren (B-11), Start ohne Hardware (B-10). |
| AP-4 | Treiber Analog+Mux, DHT (Rundenmodell), LED-Ampel | F-04, F-08 | Hardware-Smoke-Test PCB: Werte plausibel, Mux-Kanäle korrekt (Kanal 1 = 111), Settle-Zeit gemessen (Rohwert stabil nach Umschalten), Analog-Messung aller Kanäle ≤ 1,5 s, Loop-Zeit < 20 ms während der Messung, Sofortmessung von der Startseite ≤ 3 s bis zum neuen Wert, LED-Modi 0/1/2 und „nur Rot“. |
| AP-5 | `NetService` (Slots, AP + Captive-DNS, Reconnect, mDNS), `TimeService` | F-09 | Nativ: Zustandsmaschine mit Fake-WiFi. Hardware: Start ohne WLAN → AP in < 35 s erreichbar; WLAN weg/zurück; `<name>.local` auflösbar. |
| AP-6 | Web-Kern: HTTP-Adapter, Router, Auth (Hash, Sperre), Response-Sink, Assets-Einbettung (`embed_assets.py`), `/status`, `/api/v1/system`, `/getLatestValues`, `/metrics` | F-05, F-14, 3.1, 3.2 | Vergleichstest: `/getLatestValues` und `/metrics` gegen das Altgerät (gleiche Felder und Namen, gültiges JSON auch bei ±Inf). Assets mit ETag/304. Heap nach Abruf aller Seiten ≥ 20 KB. |
| AP-7 | Weboberfläche Startseite, Fußzeile, `api.js`, `ui.js`, `devicewait.js` | F-05 | JS-Tests (aus Altcode übernommen und angepasst); manuell: Blume, Blätter, Sofortmessung, Wackeln. |
| AP-8 | Admin-API (`/api/v1/config`, Secrets, Sensors) und Seiten Einstellungen, Sensoren, Display | F-13 | Jede Einstellung aus F-03 per UI änderbar und nach Neustart erhalten. B-11, B-25, B-30, B-50, B-56, B-57, B-58 nachweislich behoben. |
| AP-9 | Display-Service und Treiber, Bootlog, QR | F-07 | Nativ: Renderer-Pixel-Hashes. Hardware: Rotation, Uhr ohne NTP wird übersprungen, Bootlog. |
| AP-10 | Chronik v2 (Store, Recorder, API, Seite, CSV) | F-06 | Native Formattests + gemeinsame Vektoren mit JS. Hardware: 24 h Aufzeichnung, Neustart, Segmentreparatur, CSV-Export. |
| AP-11 | Mail (Scheduler, Templates, Envelope-Streaming, Krypto-HAL, HTTP-Client), Seiten Mail und Mailtexte | F-11, 3.3 | Krypto-Testvektor identisch zum Altcode (gleicher Schlüssel, Nonce, AAD → gleiches Chiffrat und Tag). Echte Testmail über den Mailbot. Heap-Spitze beim Versand ≤ 4 KB über Grundlast. |
| AP-12 | OTA-Service + Seite | F-12 | Update per Browser mit echtem Fortschritt; abgebrochener Upload lässt die alte Firmware lauffähig; MD5-Prüfung bei aktivierter Option. |
| AP-13 | Optionale Sensoren DS18B20, BMP280, HX711 (mit Tarieren), MH-Z19 (ISR), SDS011 (wake/warmup/sleep pro Zyklus), Serial Receiver (nicht-blockierend) | F-04 | Nativ: Protokollparser (SDS011-Rahmen, Serial-Zeilen, MH-Z19-Formel) mit Vektoren. Hardware, soweit Sensoren verfügbar (O7). Loop-Zeit bleibt < 50 ms. |
| AP-14 | Logs-Seite (Polling), Debug-Schalter, Log-Download | F-10 | Mehrere Browser gleichzeitig; nichts verloren außer gemeldeten `dropped`. |
| AP-15 | ESP32-Abnahme, Dauertest, Aufräumen: alte Verzeichnisse entfernen, README, Release-Workflow | alle | 7-Tage-Dauerlauf ESP8266 ohne Neustart, Heap-Minimum ≥ 15 KB, keine Panic/WDT; ESP32 ein Tag Dauerlauf; Release-Artefakte je Board. |

**Abnahme „funktional gleichwertig“:** Jede Zeile in Abschnitt 2 wird als Checkliste abgehakt. Abweichungen sind nur dort erlaubt, wo Abschnitt 2 oder 4 eine Verbesserung ausdrücklich nennt.

---

## 7. Offene Punkte und Annahmen

| ID | Thema | Annahme in diesem Dokument | Zu klären mit |
|---|---|---|---|
| O1 | Sicherheit | **entschieden** (E9) | – |
| O2 | Admin-Passwort-Mindestlänge | **entschieden:** keine (E10) | – |
| O3 | OTA ohne Update-Modus | Erst messen: freier Heap während des Uploads mit pausierten Sensoren. Reicht er (≥ 8 KB), entfällt der Update-Modus; sonst schlanker Update-Modus per Neustart als Fallback. | Messung in AP-12 |
| O4 | Werksreset und Chronik | **entschieden:** Chronik bleibt erhalten (E10) | – |
| O5 | Display-Bibliothek | Adafruit_SSD1306/GFX beibehalten oder eigener Treiber + Font mit Umlauten. Entscheidung nach Flash/RAM-Messung. | AP-9 |
| O6 | Versionierung | **entschieden:** bleibt wie heute (E10) | – |
| O7 | Optionale Sensoren | **entschieden:** Hardware für alle vorhanden; zuerst Analog und DHT, die übrigen in AP-13 (E3) | – |
| O8 | Feldname/Kanalschlüssel | Kanalschlüssel `<ID>_<idx>` bleibt (E8). Der Kanalindex der Chronik wird stabil (5.8); das ändert nichts an externen Schnittstellen. | – |
| O9 | `/measure` (Altpfad) | **entschieden:** kein Alias (E10) | – |
| O10 | Konfig-Import alter Dateien | **entschieden:** muss möglich sein (E2, Tabelle in 2.3) | – |
| O11 | Messzyklus | **entschieden:** zentraler Mess-Scheduler mit Ressourcen, Runden und Median, Sofortmessung als Priorität, ohne Ausrichtung auf volle Minuten (2.4, 5.6) | – |

---

## 8. Anhang: Referenzkarte Altcode → Funktion

Wo die Referenz für eine Funktion steht. Pfade relativ zu `Pflanzensensor/`.

| Funktion | Altcode | Tests (Altcode) |
|---|---|---|
| Bootfolge, Hauptschleife | `src/main.cpp`, `src/main_init.cpp`, `src/main_wifi.cpp`, `src/main_sensors.cpp`, `src/main_web.cpp`, `src/main_led.cpp` | – |
| Build-Konfiguration, Defaults | `src/configs/config_example.h`, `src/configs/config.h` (VERSION), `src/sensors/sensor_config.h` | – |
| Konfiguration (Werte, Grenzen, Defaults) | `src/managers/manager_config*.{h,cpp}`, `manager_config_preferences.cpp`, `manager_config_validator.cpp`, `src/configs/config_validation_rules.h` | – |
| Konfig-Export/-Import | `src/managers/manager_config_persistence.cpp` (`backupPreferencesToFile`, `restorePreferencesFromJson`), `src/web/handler/admin_handler_config.cpp` | – |
| Sensor-Basis, Status | `src/sensors/sensors.{h,cpp}`, `sensor_types.h` | `test/test_messzustand` |
| Messzyklus, Slot, Sofortmessung | `src/sensors/sensor_measurement_cycle*.cpp`, `sensor_measurement_state.h`, `sensor_manager_limiter.h`, `src/managers/manager_sensor*.{h,cpp}` | `test/test_measurement_cycle`, `test_limiter`, `test_sensor_preemption`, `test/shim/FakeSensor.h` |
| Treiber | `src/sensors/sensor_{analog,analog_multiplexer,autocalibration,dht,ds18b20,sds011,mhz19,hx711,bmp280,serial_receiver}.*`, `sensor_factory.cpp` | – |
| Kanalzustand persistieren | `src/managers/manager_sensor_persistence.cpp`, `pending_update_queue.h` | `test/test_pending_update_queue` |
| Chronik | `src/chronik/*`, `src/utils/chronik_{format,budget,segments}.h`, `src/web/handler/chronik_handler.cpp`, `data/js/chronik.js` | `test/test_chronik_*`, `test/js/chronik.test.mjs` |
| Display | `src/managers/manager_display.cpp`, `src/display/*` | – |
| LED-Ampel | `src/managers/manager_led_traffic_light.cpp`, `src/led-traffic-light/*` | – |
| WLAN, AP, mDNS | `src/utils/wifi.cpp`, `src/utils/mdns_name.h` | `test/test_mdns_name` |
| Zeit, Logging | `src/logger/logger.{h,cpp}`, `src/utils/helper.cpp` | – |
| Live-Log | `src/web/handler/log_handler.cpp`, `src/web/services/websocket.cpp`, `data/js/logs.js` | – |
| Mail | `src/mail/*`, `src/utils/mail_scheduler.h`, `src/utils/mail_template.h`, `src/utils/base64.h`, `src/web/handler/admin_email_handler*.cpp`, `data/js/mailvorlagen.js` | `test/test_mail_scheduler`, `test_mail_template`, `test/js/mailvorlagen.test.mjs` |
| OTA | `src/web/handler/web_ota_handler.cpp`, `data/js/ota.js`, `data/js/devicewait.js` | `test/js/devicewait.test.mjs` |
| Web-Kern, Auth, Assets | `src/web/core/*`, `src/web/handler/base_handler.h` | `test/test_web_auth_decision`, `test_request_throttle` |
| Startseite | `src/web/handler/startpage_handler.cpp`, `sensor_handler.cpp`, `data/js/sensors.js`, `data/css/start.css` | `test/js/sensors.test.mjs` |
| Admin-Seiten | `src/web/handler/admin_handler*.cpp`, `admin_sensor_handler*.cpp`, `admin_display_handler.cpp`, `data/js/admin*.js`, `data/css/admin.css` | `test/js/admin-helpers.test.mjs`, `sensor-range.test.mjs` |
| Metriken | `src/web/handler/web_metrics_handler.cpp`, `src/metrics/prometheus_metrics.cpp` | – |
| CRC, Speicher, Plattform | `src/utils/crc32.h`, `memory_manager.cpp`, `critical_section.h`, `safe_yield.h`, `platform_compat.h` | `test/test_crc32` |
| Build, CI, Flash-Layout | `platformio.ini`, `eagle.flash.4mmore.ld`, `compress_assets.py`, `generate_md5.py`, `bump_version.py`, `check_stack.sh`, `check_warnings.sh`, `.github/workflows/ci.yml`, `flake.nix` | – |
| Hardware | `Dokumentation/Pflanzensensor PCB/Pflanzensensor_Schematic.pdf` | – |
