# Abnahme auf Hardware (refactor.md Abschnitt 6)

Native Tests (`pio test -e native`), JS-Tests und der Bau beider Plattformen
laufen in der CI. Diese Liste enthält die Punkte, die nur am Gerät geprüft
werden können. Serielles Log mit 115200 Baud mitschreiben
(`pio device monitor -e esp8266_pcb`). Alles über HTTP Prüfbare erledigt
`tools/acceptance.py <ip> --password <pw> --loop 120 --robust`.

**Stand 05.11.2026** – Gerät `pcb_v1` (NodeMCU, DHT11, zwei Analogkanäle),
vorher Firmware 2.x, Prüfung über das Netz (kein Zugriff auf Taster, Stecker,
Display und LED). `[x]` = am Gerät geprüft, `[~]` = teilweise, `[ ]` = offen.

## AP-1 Plattform, Log
- [~] Bootmeldung „Pflanzensensor 3.x.y (pcb_v1, …) startet“ – im Live-Log
  geprüft (Serial nicht); Baudatum stimmt jetzt auch aus einer Nix-Shell
- [x] Datei-Log einschalten, mehr als 16 KB erzeugen: `/log.txt` rotiert, neuere Hälfte bleibt
- [x] Debug „RAM“ an: Loop-Zeiten im Leerlauf < 20 ms. Debug „RAM“ meldet
  jetzt alle 30 s die längste Schleife („Loop max. … ms (Task)“). Gemessen:
  17–19 ms; Ausnahme sind Flash-Schreibvorgänge (`persist`, 130–480 ms),
  die refactor.md 5.5 ausnimmt
- [~] Drei schnelle Neustarts: über die API geprüft (Nr. 2 → Chronik aus,
  Nr. 3 → Datei-Log aus und gelöscht, nach 60 s Laufzeit zurückgesetzt, auch
  nach einem OTA-Start); Reset-Taste und Kaltstart (Stecker) nicht geprüft

## AP-4 Analog + Mux, DHT, LED
- [~] Werte plausibel (Licht nachts 3,5 %, Bodenfeuchte, 19 °C); Zuordnung
  Kanal 1 = Adresse 111 nicht physisch geprüft
- [ ] Settle-Zeit prüfen: Rohwert stabil direkt nach dem Umschalten (sonst
  `AnalogMux::kSettleUs` erhöhen) – über das Netz nur indirekt: Rohwert
  Kanal 0 schwankt über Stunden um ±4 Counts
- [x] Analog-Messung aller Kanäle ≤ 1,5 s (0,7–0,9 s inkl. HTTP); Loop-Zeit < 20 ms
- [~] Sofortmessung: neuer Wert ≤ 3 s (Analog 0,9 s, DHT 0,3 s); „Blatt wackelt“ nicht gesehen
- [~] DHT-Fehler: Kanal zeigt `error` und `null` (nie 0), Sensor bleibt in
  der Liste und erholt sich – beobachtet bei blockierter Schleife, nicht durch Abziehen
- [ ] LED-Modi 0/1/2 und „Nur Rot“; Wechsel auf 0 schaltet sofort aus

## AP-5 Netz
- [ ] Ohne WLAN: AP „<Gerätename>“ in < 35 s, Captive-Portal öffnet `/admin`
- [ ] WLAN im AP eintragen: spätestens nach 5 min (rotierend) verbunden, AP aus
- [ ] Router aus/an: Wiederverbindung ohne Neustart
- [~] `<name>.local`: nach Umbenennen meldet das Log den neuen Namen
  (`balkon-sued.local`); Auflösung nicht geprüft
- [x] NTP: Uhrzeit wenige Sekunden nach der WLAN-Verbindung gestellt

## AP-6 Web-Kern
- [ ] `/getLatestValues` und `/metrics` gegen ein Gerät mit Firmware 2.x vergleichen (Felder, Namen)
- [x] Assets mit ETag/304
- [x] Heap nach Abruf aller Seiten ≥ 20 KB (~29 KB frei, größter Block ~29 KB)
- [x] 20 parallele Clients, 120 gemischte Abrufe mit 40 Threads: alle beantwortet, Heap stabil
- [x] Langsame und kaputte Clients (halbe Kopfzeilen, 6000-Zeichen-URL,
  3-KB-Authorization, falsche Content-Length, Müll): Schleife ≤ 51 ms,
  andere Clients warten ≤ 0,5 s, kein Heap-Verlust (vorher: 10 s Stillstand,
  9 KB Heap dauerhaft weg)

## AP-8 Admin
- [x] Jede Einstellung ändern, Neustart, Wert bleibt (Gerät, Display, LED,
  Log, Mail, Sensoren, Kanäle, Passwort, Mailschlüssel, Vorlagen, Stil)
- [x] „Skala invertieren“ wirkt sofort auf den Prozentwert (B-25/B-56)
- [x] Altes `config.json` importieren (Testdatei aus `test/vectors`, ohne
  WLAN-Teil): 50 Werte übernommen, unbekannte Sensoren und ungültige
  Schwellen gemeldet
- [x] Notfallpasswort funktioniert und wird im Log gemeldet; Sperre nach Fehlversuchen (429, 30 s)

## AP-9 Display
- [ ] Rotation in der eingestellten Dauer; Uhr ohne NTP wird übersprungen; 12h zeigt AM/PM
- [ ] Bootlog mit WLAN-Versuchen bzw. AP-Anleitung; QR-Code scannbar
- [ ] **Neu prüfen:** Das Bild wird jetzt in zwei Hälften übertragen
  (je 4 Seiten pro 100-ms-Takt, vorher 53 ms am Stück). Bildwechsel
  sollten sauber aussehen

## AP-10 Chronik
- [~] Aufzeichnung über mehrere Neustarts und OTA-Updates durchgehend
  (CSV 400+ Zeilen); 24 h noch offen
- [~] Startreparatur: kürzte beim ersten Start das alte 2.x-Segment
  („Angeschnittener Rahmen … 2033 auf 0 Bytes“); Stromausfall nicht geprüft
- [x] CSV-Export vollständig
- [x] Platz: Nach dem Löschen der 2.x-Altlasten 28 Segmente (vorher 0–9,
  zeitweise ganz aus)

## AP-11 Mail
- [ ] Testmail über den echten Mailbot kommt an (Betreff mit „[Gerätename]“) –
  Umschlag erreicht den Mailbot, mit Test-Geräte-ID kommt 401 zurück und wird
  sauber gemeldet; Zustellung braucht echte Geräte-ID und Schlüssel
- [x] Debug „Mail“: Heap-Spitze 3,5 KB über Grundlast (≤ 4 KB)

## AP-12 OTA
- [x] Update mit MD5 (über die API wie der Browser), Gerät kommt nach ~25 s
  mit neuem Baudatum zurück; Fortschrittsbalken im Browser nicht gesehen
- [x] Upload abbrechen: alte Firmware läuft weiter, Sensoren messen wieder
- [x] „Prüfsumme verlangen“ an: ohne/falsche MD5 abgelehnt; keine Firmware-Datei abgelehnt
- [x] O3: freier Heap während des Uploads ≥ 8 KB (gemessen 14,7 KB; steht
  jetzt im Log „Update erfolgreich (…, Heap min. … B)“)

## AP-13 Optionale Sensoren
Für jeden Sensor ein eigenes Profil anlegen (Pinkonflikte, refactor.md 2.1).
- [ ] DS18B20 (eine Konversion für alle), BMP280, HX711 (Tarieren, Nullpunkt bleibt nach Neustart)
- [ ] MH-Z19 PWM: 180 s Warmup, danach Werte; Loop-Zeit < 50 ms
- [ ] SDS011: Wecken, 30 s Warmup, 3 Runden, Schlafen; andere Sensoren messen währenddessen
- [ ] Serial Receiver: 7 Kanäle, Timeout ohne Blockieren
- [ ] DHT22 (ESP32-Profil): eigener Treiber mit 1,1-ms-Startsignal

## AP-14 Logs
- [x] Drei Leser gleichzeitig, nichts verloren (Skript); zwei Browser nicht geprüft

## Webflasher (USB)
- [ ] Neues Gerät: über die Pages-Seite flashen, WLAN im Assistenten
  eintragen → verbindet sich, „Gerät besuchen“ öffnet die Oberfläche
- [ ] Eingerichtetes 3.x-Gerät: flashen ohne „Gerät löschen“ → Einstellungen,
  Chronik und Passwort unverändert, kein WLAN-Schritt
- [ ] Gerät mit 2.x: flashen, WLAN eintragen, alte `config.json` importieren
- [ ] ESP32-DevKit: alle vier Teile, startet aus app0
- [ ] Improv bei laufendem Log (Debug an): Assistent erkennt das Gerät trotzdem

## AP-15 Dauertest und Aufräumen
- [ ] 7 Tage ESP8266 ohne Neustart, Heap-Minimum ≥ 15 KB, keine Panic/WDT –
  30-min-Lauf nach allen Korrekturen: kein Neustart, Heap-Minimum 28,2 KB (größter Block ≥ 24 KB), 0 DHT-Fehlrunden, Schleife Median 18 ms
- [ ] 1 Tag ESP32
- [x] `Pflanzensensor/`, alte Tests, `ci.yml` und alte Build-Skripte entfernt (05.11.2026)

## Gefunden und behoben (05.11.2026)

| Befund | Ursache | Behebung |
|---|---|---|
| Chronik nur 0–9 Segmente, nach Neustart ganz aus | ~750 KB Dateien von Firmware 2.x im LittleFS | `storage::removeForeign()` löscht beim Start alles, was nicht zu Firmware 3 gehört |
| Budget schrumpft bei jeder Rotation | eigene Segmente nach Bytes statt nach belegten Blöcken gezählt | `kSegmentFootprint` (8 KB je Segment); erneute Prüfung alle 10 min, wenn aus; Warnung „Chronik aus“ |
| Task `system` 80 ms alle 30 s | `LittleFS.info()` durchläuft alle Blöcke | nur noch auf Anfrage (`/api/v1/config`) |
| Datei-Log: jeder Flush öffnet die Datei zweimal; Rotation 128-B-Häppchen | – | Größe zwischengespeichert, Rotation mit 1-KB-Puffer |
| Display-Wechsel 53 ms | ganzes Bild (1 KB über I2C) in einem Schritt | 4 Seiten je Takt, 32-Byte-Transfers |
| DHT 23–41 ms | DHTesp: `delay(18)` + WLAN-Verarbeitung | eigener Treiber, Startsignal über zwei Schritte, Sensor-Task alle 10 ms |
| Ein langsamer Client legt das Gerät 10 s lahm | ESP8266WebServer liest blockierend | Vorfilter: erst vollständige Anfragen annehmen; 414/413/400 sofort |
| 9 KB Heap dauerhaft weg nach langer URL/Kopfzeile | `String` behält seine Kapazität | übergroße Puffer nach jeder Anfrage freigeben |
| Baudatum „Jan 1 1980“ | Nix setzt `SOURCE_DATE_EPOCH=315532800` | `tools/embed_assets.py` schreibt `src/core/build_date.h` |
| MD5-Fehler als abgeschnittener englischer Text | Core-Meldung > 64 Zeichen | häufige OTA-Fehler auf Deutsch |
| Nach einem OTA-Update zählte der Boot-Schutz falsch weiter („Mehrfachstart“, Chronik aus) | Boot-Schutz lag im RTC-Speicher, den eboot für das OTA-Kommando nutzt (0x60001200) | RTC-Block 128 statt 64 |
| Warnung „Rohwert außerhalb … begrenzt“ bei jeder Messung (Lichtsensor nachts) | – | einmal je Kanal, bis der Wert wieder im Bereich liegt |
| Sensorseite: Regler ohne Zahlen, Wert/Min/Max bleiben stehen, Layout zerfällt | Regler ohne Beschriftung neu geschrieben; `.card label` (display: block) überstimmte die Zeilen | Zahlen an den Griffen, Skala mit Teilstrichen, aktueller Wert und beobachteter Bereich laufend; Raster je Skala, Pfeiltasten; Karten als Zwei-Spalten-Tabelle |
| Alle Seiten: Desktop riesige Schrift und Knöpfe, Handy winzig; Knöpfe eingerückt; Name im Footer abgeschnitten | Schriftgröße hing an der Fensterbreite (vw), Knöpfe mit `margin`, Footer mit `max-width` + `ellipsis` | feste Grundgröße (16/15 px), Karten im Raster, Knöpfe in Zeilen, Footer bricht um, `text-size-adjust` gegen Font-Boosting |
| Logs nur mit Login | – | `/logs`, `/api/v1/logs{,/file}` öffentlich; `/api/v1/logs/settings` (GET/PATCH) nimmt nur Level und Debug-Schalter an |
| Log-Level „ändert sich nicht“ | Debug-Schalter lieferten unabhängig vom Level; Logs-Seite loggte ihre eigenen Abrufe | Level ist harte Schwelle, Schalter wählen nur bei DEBUG die Bereiche; `/api/v1/logs*` nicht mehr im Web-Log |
| Messzyklus und Display ohne aussagekräftige Logs | – | Runden, Median, Ergebnis je Kanal, Statuswechsel (INFO), Dauer und nächste Messung; Display nennt den Inhalt |
| Benutzername `admin` ohne Nutzerverwaltung | Basic-Auth | Anmeldeseite nur mit Passwort, Sitzungs-Cookie; Basic-Auth für Skripte ignoriert den Namen |
| `pio test -e native` findet `unity.h` nicht | LDF folgt `test_support.h` nicht | Unity explizit in `lib_deps` |

Bewusst nicht geändert: Nach „Skala invertieren“ oder neuen Rohgrenzen
bleiben die beobachteten %-Extremwerte stehen (sie stammen aus der alten
Abbildung); sie lassen sich auf der Sensorseite zurücksetzen.
