# Pflanzensensor – Firmware 3

Zielhardware ist das eigene PCB mit ESP8266 (NodeMCU v2, 4 MB). Die Firmware
baut und läuft zusätzlich auf ESP32. Die Firmware 2.x findet sich in der
Git-Historie (bis Commit `d7c3ba5`).

## Bauen und flashen

```sh
cd firmware
pio run -e esp8266_pcb               # Firmware für das PCB
pio run -e esp8266_pcb -t upload     # flashen (USB)
pio run -e esp32_devkit              # Referenzprofil ESP32
pio test -e native                   # Unit-Tests ohne Hardware
tools/run_js_tests.sh                # Tests der Weboberfläche
```

Updates bestehen nur aus `firmware.bin` (die Weboberfläche ist gzip-komprimiert
eingebettet, E6). LittleFS enthält nur Daten und wird nie überschrieben.

Ohne PlatformIO-Registry (offline) laufen die nativen Tests auch so:
`UNITY=… ARDUINOJSON=… QRCODE=… tools/run_native_tests.sh` (mit
`EXTRA_FLAGS=-fsanitize=address,undefined` auch mit Sanitizern).

## Webflasher und Releases

Ohne Programmierumgebung flasht man über den Browser (Chrome, Edge, Opera am
Computer, USB-Datenkabel): **https://fabmobil.github.io/Pflanzensensor/**
(`webflasher/`, gebaut von `.github/workflows/webflasher.yml`, Flashen mit
[ESP Web Tools](https://esphome.github.io/esp-web-tools/)).

- Es wird nur die Firmware geschrieben; Einstellungen, Chronik und Log
  bleiben, solange man „Gerät löschen“ nicht ankreuzt.
- Danach fragt der Assistent über Improv Serial (`src/core/net/improv.*`,
  `src/services/improv_service.*`) nach dem WLAN, wenn das Gerät noch keins
  hat. Das landet in Slot 1; überspringen geht. Eingerichtete Geräte werden
  nicht gefragt („WLAN ändern“ geht trotzdem).
- Releases: Liegt auf `main` eine `VERSION` ohne Tag, legt der Job `release`
  in `.github/workflows/firmware.yml` das Release `v<VERSION>` an (Dateien aus
  `tools/release_assets.sh`) und baut den Webflasher neu. Die Seite kopiert
  die Release-Dateien zu sich, weil Browser Release-Downloads nicht direkt
  laden dürfen (CORS). Firmware-2.x-Releases tauchen dort nicht auf.
- Einmalig im Repository: Settings → Pages → Source „GitHub Actions“.
- Lokal ansehen: `python3 webflasher/build_site.py --out /tmp/site --offline
  --local firmware/dist --esp-web-tools <npm esp-web-tools>/dist/web`, dann
  `python3 -m http.server -d /tmp/site` (USB geht nur über localhost/HTTPS).

## Tests

| Suite | Inhalt |
|---|---|
| `test/native/test_core`, `test_core2` | Typen, JSON-Writer, CRC/Base64/SHA-256, mDNS-Name, Canvas, Chronik-Format-Grenzen, Mail-Scheduler-Zweige |
| `test_sensors` | Mess-Scheduler (Ressourcen, Warmup, Fristen, Backoff, Drift, Überlauf), Median, Autokalibrierung, Protokolle (SDS011, MH-Z19, Serial, DHT-Rahmen) |
| `test_config` | Schema, Roundtrip, `.tmp`-Wiederherstellung, Export ohne Geheimnisse, Import neu und alt |
| `test_chronik`, `test_mail`, `test_mail2` | Chronik-Dienst, Mail-Vorlagen/-Krypto/-Versand inkl. aller Fehlerpfade |
| `test_services`, `test_services2` | Log (Rotation, Größen-Cache), Netz-Zustandsmaschine, Boot-Schutz, Zustand, Display (seitenweise Übertragung), OTA, LED, Zeit, Task-Takt, Chronik-Budget, Löschen der 2.x-Altlasten |
| `test_web`, `test_web_api` | Jeder HTTP-Endpunkt mit Fehlerpfaden, Assets/ETag, Auth-Sperre, Uploads |
| `test_api_contract` | Vertrag C++ ↔ Weboberfläche: echte API-Antworten in `test/vectors/api/` (neu schreiben mit `PS_WRITE_FIXTURES=1`) |
| `test_fuzz` | Eigenschafts- und Fuzz-Tests (Rundläufe, Müll-Eingaben, Konfig-Invarianten, Scheduler-Chaos) |
| `test_drivers` | Analog/Mux, LED-Ampel, MH-Z19-ISR gegen einen Arduino-Shim |
| `test/js/*.test.mjs` | Gemeinsame Vektoren, alle Seiten im Mini-DOM mit den API-Fixtures, Chronik-Diagramm, DeviceWait |

Abdeckung messen: `tools/coverage.sh` (gcovr, Bericht in `.pio/coverage/`).
Mit Sanitizern: `EXTRA_FLAGS=-fsanitize=address,undefined tools/run_native_tests.sh`.

## Aufbau

| Verzeichnis | Inhalt |
|---|---|
| `boards/` | Board-Profile (`pcb_v1.h`, `esp32_devkit.h`, `native_test.h`), `board.h` (Kanal-Layout zur Compile-Zeit), `profile_check.h` (Pinkonflikte als `static_assert`) |
| `src/core/` | Fachlogik ohne Arduino: Typen, JSON-Writer, Konfig-Schema, Import der Altkonfiguration, Mess-Scheduler, Status, Autokalibrierung, Chronik v2, Mail-Scheduler/-Vorlagen/-Umschlag, Display-Renderer |
| `src/hal/` | Schnittstellen zur Plattform (System, Dateisystem, RTC-RAM, WLAN, HTTP, TCP, Krypto, OTA, Display) |
| `src/platform/` | `arduino/` (ESP8266 und ESP32, einzige Stelle mit Plattform-`#ifdef`s), `native/` (Fakes für Tests), `progmem.h` |
| `src/drivers/` | Sensor- und Peripherietreiber (Analog+Mux, DHT, DS18B20, SDS011, MH-Z19, HX711, BMP280, Serial Receiver, SSD1306, LED-Ampel) |
| `src/services/` | Dienste (Konfiguration, Sensoren, Zustand, Chronik, Display, LED, Netz, Zeit, Log, Mail, OTA, Metriken, Systeminfo) |
| `src/web/` | Router mit statischer Routentabelle, Auth, API-Handler, Uploads; `assets.h` wird erzeugt |
| `src/app/` | Composition Root (`main.cpp`), Task-Takt, Boot-Schutz |
| `web/` | Statische Seiten, JavaScript, CSS, Bilder |
| `tools/` | `embed_assets.py`, `gen_md5.py`, `bump_version.py`, `check_stack.sh`, `check_forbidden.sh`, `check_profile_conflict.sh`, `gen_vectors.cpp`, Testskripte |
| `test/native/` | Unity-Tests; `test/js/` node-Tests; `test/vectors/` gemeinsame Testvektoren C++/JS |

## Laufzeitmodell

Kooperative Hauptschleife mit Task-Tabelle (`app/main.cpp`): Web jede Schleife,
Netz/Sensoren/Display/Mail alle 100 ms, LED/Zeit/Persistenz jede Sekunde,
Systemstatistik alle 30 s. Module sind über eine `EventQueue` entkoppelt.
**Flash wird nur in `persist()` geschrieben** (Konfiguration mit 2 s Debounce,
Kanalzustand höchstens alle 15 min, Chronik höchstens alle 5 min, Datei-Log).
Neustart und Werksreset passieren erst, nachdem die HTTP-Antwort raus ist und
alles gesichert wurde.

Messungen laufen über den zentralen Mess-Scheduler (`core/sensors/scheduler.h`):
Prepare (begin/wake/Warmup ohne Ressource) → Runden (Ressource nur während der
Runde) → Median je Kanal. Eine Frist pro Auftrag; Fehlermodell Ok → Degraded →
Failed mit Backoff bis 30 min.

## Dateien auf dem Gerät (LittleFS, nur Wurzelverzeichnis)

`/device.cfg`, `/sensors.cfg`, `/secrets.cfg` (JSON, atomar über `.tmp`),
`/state.bin` (binär mit CRC32), `/mail.tpl` (Format `#MV2`), `/boot.cnt`,
`/log.txt`, `/chr_XXXXXXXX.dat` (Chronik v2).

## HTTP-API

Kompatibel geblieben (E8): `GET /getLatestValues`, `GET /metrics`,
Mailbot-Protokoll. Öffentlich: `/`, `/chronik`, `/logs`, `/status`,
`/api/v1/system`, `POST /api/v1/measure`, `/api/v1/chronik/{data,info,export.csv}`,
`/api/v1/logs`, `/api/v1/logs/file` und `/api/v1/logs/settings` (GET/PATCH,
nur Log-Level und Debug-Schalter; Datei-Log und alles andere nur über
`/api/v1/config`). Alles andere – die Admin-Seiten und ihre APIs – verlangt
die Anmeldung. Es gibt nur ein Passwort, keinen Benutzernamen:
Der Browser meldet sich auf `/login` an (`POST /api/v1/login {"password":…}`,
Sitzungs-Cookie `ps_session`, HttpOnly, SameSite=Strict, nach einem Neustart
neu anmelden; `POST /api/v1/logout`). Skripte dürfen HTTP-Basic-Auth schicken,
der Benutzername ist egal. Admin-Seiten ohne Anmeldung leiten auf `/login` um,
die API antwortet mit 401 ohne `WWW-Authenticate`. Geschützt: `/api/v1/config`
(GET/PATCH), `/api/v1/secrets/…`, `/api/v1/sensors` (GET/PATCH, Aktionen unter
`/api/v1/sensors/<ID>/…`), `/api/v1/mail/…`,
`/api/v1/config/{export,import}`, `/api/v1/system/{reboot,factory-reset}`,
`/api/v1/ota`. Tabelle: `src/web/router.cpp`.

Fehlerformat: `{"ok":false,"error":"<code>","message":"…","fields":{…}}`.

## Entscheidungen aus den offenen Punkten

- **O3 (OTA ohne Update-Modus):** Upload im Normalbetrieb. Sensoren werden
  pausiert und alles wird gesichert; es gibt keinen Neustart in einen Update-Modus.
  Gemessen: mindestens ~13 KB freier Heap während des Uploads (Ziel ≥ 8 KB).
- **O5 (Display):** eigener SSD1306-Treiber mit 1-KB-Framebuffer und eigenem
  5×7-Font (mit ä ö ü ß °). Spart die Adafruit-Bibliotheken; die Bildschirme
  sind reine Funktionen und nativ per Pixel-Hash getestet.
- **Live-Log:** Polling statt WebSocket.
- **DHT ohne Bibliothek:** DHTesp wartet für das Startsignal des DHT11 mit
  `delay(18)`; mit WLAN-Last wurden daraus 35–40 ms Schleifenzeit. Der eigene
  Treiber zieht die Leitung in einem Schritt auf LOW und liest 20 ms später
  (~5 ms ohne Interrupts); der Sensor-Task läuft dafür alle 10 ms.
- **Web-Vorfilter (ESP8266):** ESP8266WebServer liest Anfragen blockierend
  (bis 2 × 5 s bei halben Kopfzeilen). Eine Anfrage geht erst an die
  Bibliothek, wenn Kopf und kleiner Rumpf vollständig empfangen sind;
  überlange URLs (414) und zu große Rümpfe (413) werden sofort abgewiesen.

## Abnahme am Gerät

Alles, was über HTTP prüfbar ist, erledigt `tools/acceptance.py` (nur
Python-Standardbibliothek):

```
python3 tools/acceptance.py <ip> --password <pw> [--loop 120] [--robust] [--reboot] [--ota firmware.bin] [--long 600]
```

Ausgabe: Markdown-Bericht auf stdout und `acceptance-report.json`.

**Beim ersten Start nach Firmware 2.x** löscht die Firmware alle Dateien,
die nicht zu ihr gehören (alte Web-Dateien, `/nvs`, `/config`, …; ~750 KB).
Eine Migration gibt es nicht – die alte Konfiguration also
vorher mit „Konfiguration herunterladen“ sichern und danach importieren.
