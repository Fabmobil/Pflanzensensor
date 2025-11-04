# Preferences-basierte Konfigurationsverwaltung

## Überblick

Das Pflanzensensor-Projekt verwendet die Preferences-Bibliothek zur Speicherung der Konfiguration im EEPROM. Dies bietet eine robustere und effizientere Alternative zu JSON-Dateien und **überlebt Filesystem-Flashes**, da die Preferences im EEPROM-Bereich (0x405FB000) gespeichert werden, der getrennt vom LittleFS-Bereich (0x40512000-0x405FA000) ist.

## EEPROM-Speicherplatz

**Verfügbarer EEPROM:** 4.096 Bytes (4KB)

**Speicher-Optimierung:**
Aufgrund der 4KB-Begrenzung werden nur laufzeitveränderbare Daten im EEPROM gespeichert. Statische Metadaten (Sensornamen, Messnamen, Einheiten) werden im Code definiert.

**Gespeichert in Preferences (~3,3KB):**
- Allgemeine Einstellungen, WiFi, Display, Log, LED, Debug (~570 Bytes)
- Sensor-Messintervalle und Fehlerzustände
- Messwert-Schwellenwerte und Aktivierungszustände
- Analog-spezifisch: Min/Max, Kalibrierung, Raw-Werte

**Nicht gespeichert (im Code definiert):**
- Sensornamen (basierend auf Sensortyp/ID)
- Messwertnamen (pro Sensortyp definiert)
- Einheiten (pro Sensortyp definiert)
- Feldnamen (verwenden Sensornamen)

## Namespace-Organisation

Die Konfiguration ist in logische Namespaces unterteilt:

### In Preferences gespeichert

#### 1. General Settings (`general`)
- Gerätename (`device_name`)
- Admin-Passwort (`admin_pwd`)
- MD5-Verifizierung (`md5_verify`)
- Datei-Logging (`file_log`)
- Flower-Status-Sensor (`flower_sens`)

#### 2. WiFi Settings (`wifi`)
- SSID 1-3 (`ssid1`, `ssid2`, `ssid3`)
- Passwörter 1-3 (`pwd1`, `pwd2`, `pwd3`)

#### 3. Display Settings (`display`)
- IP-Bildschirm anzeigen (`show_ip`)
- Uhr anzeigen (`show_clock`)
- Blumenbild anzeigen (`show_flower`)
- Fabmobil-Bild anzeigen (`show_fabmobil`)
- Bildschirmdauer (`screen_dur`)
- Uhrformat (`clock_fmt`)

#### 4. Log Settings (`log`)
- Log-Level (`level`)
- Datei-Logging aktiviert (`file_enabled`)

#### 5. LED Traffic Light Settings (`led_traf`)
- Modus (`mode`): 0 = aus, 1 = alle Messungen, 2 = einzelne Messung
- Ausgewählte Messung (`sel_meas`)

#### 6. Debug Settings (`debug`)
- RAM-Debug (`ram`)
- Messzyklus-Debug (`meas_cycle`)
- Sensor-Debug (`sensor`)
- Display-Debug (`display`)
- WebSocket-Debug (`websocket`)

#### 7. Sensor Settings (pro Sensor-Namespace: `s_<sensorId>`)

**Gespeicherte Daten:**
- Messintervall (`meas_int`)
- Fehler-Status (`has_err`)
- Initialisiert-Flag (`initialized`)

**Pro Messung (Präfix: `m0_`, `m1_`, etc.):**
- Aktiviert (`m0_en`, `m1_en`, ...)
- Min/Max-Werte (`m0_min`, `m0_max`, ...)
- Schwellenwerte:
  - `m0_yl` (yellowLow)
  - `m0_gl` (greenLow)
  - `m0_gh` (greenHigh)
  - `m0_yh` (yellowHigh)

**Analog-spezifisch (nur für ANALOG-Sensoren):**
- Invertierung (`m0_inv`, `m1_inv`, ...)
- Kalibrierungsmodus (`m0_cal`, `m1_cal`, ...)
- Autocal-Dauer (`m0_acd`, `m1_acd`, ...)
- Raw Min/Max (`m0_rmin`, `m0_rmax`, ...)

**Nicht gespeichert (im Code definiert):**
- Sensorname (basiert auf Sensor-ID/Typ)
- Messwertnamen (pro Sensortyp vordefiniert)
- Einheiten (pro Sensortyp vordefiniert)
- Feldnamen (verwenden Sensornamen)

### Beispiel: Sensor-Namespace für ANALOG_1

```
Namespace: s_ANALOG_1
Keys:
  - initialized: true
  - meas_int: 60000 (Messintervall in ms)
  - has_err: false
  - m0_en: true
  - m0_min: 0.0
  - m0_max: 1023.0
  - m0_yl: 200.0
  - m0_gl: 300.0
  - m0_gh: 800.0
  - m0_yh: 900.0
  - m0_inv: false
  - m0_cal: false
  - m0_acd: 86400
  - m0_rmin: 0
  - m0_rmax: 1023
  ... (m1_, m2_, etc. für weitere Messungen)
```

## Speicherort und Filesystem-Sicherheit

### EEPROM vs. LittleFS

Die `vshymanskyy/Preferences` Bibliothek für ESP8266 speichert Daten im **EEPROM-Bereich** des Flash-Speichers:

- **EEPROM-Bereich:** `0x405FB000` (4KB) - Hier werden Preferences gespeichert
- **LittleFS-Bereich:** `0x40512000 - 0x405FA000` (~1MB) - Hier werden Dateien gespeichert

**Wichtig:** Da Preferences im EEPROM-Bereich gespeichert werden, bleiben sie bei einem Filesystem-Flash (der nur den LittleFS-Bereich betrifft) **erhalten**. Dies macht die Konfiguration robuster gegenüber Dateisystem-Updates.

## Boot-Prozess

1. Das System prüft beim Start, ob Preferences vorhanden sind
2. Wenn nicht vorhanden:
   - Werden Preferences mit Standardwerten aus `config_example.h` initialisiert
3. Wenn vorhanden:
   - Werden die gespeicherten Werte geladen
4. Statische Metadaten (Sensornamen, Einheiten) werden im Code definiert

**Keine automatische Migration:** Es gibt keine automatische Migration von JSON zu Preferences. Die Preferences werden beim ersten Boot mit Standardwerten initialisiert.

## Verwendung in Code

### Laden von Einstellungen

```cpp
String deviceName, adminPassword;
bool md5Verification, fileLoggingEnabled;

auto result = PreferencesManager::loadGeneralSettings(
    deviceName, adminPassword, md5Verification, fileLoggingEnabled);

if (result.isSuccess()) {
    // Einstellungen erfolgreich geladen
}
```

### Speichern von Einstellungen

```cpp
auto result = PreferencesManager::saveGeneralSettings(
    deviceName, adminPassword, md5Verification, fileLoggingEnabled);

if (result.isSuccess()) {
    // Einstellungen erfolgreich gespeichert
}
```

### Factory Reset

```cpp
auto result = PreferencesManager::clearAll();
```

Dies löscht alle Preferences, aber nicht die `sensors.json` Datei.

## Vorteile gegenüber JSON

1. **Effizienz**: Schnellerer Zugriff auf einzelne Werte
2. **Robustheit**: Weniger anfällig für Korruption
3. **Speicher**: Geringerer RAM-Verbrauch beim Lesen/Schreiben
4. **Atomare Updates**: Einzelne Werte können ohne Laden der gesamten Konfiguration aktualisiert werden
5. **Filesystem-Unabhängig**: Überlebt Filesystem-Flashes, da im EEPROM gespeichert

## Debugging

Alle Preferences-Operationen werden mit deutschen Logger-Meldungen protokolliert:
- Erfolgreiche Lade-/Speichervorgänge
- Fehler mit detaillierten Meldungen
- Initialisierungs-Status

## Datenspeicher-Limits

- Namespace-Name: max. 15 Zeichen
- Key-Name: max. 15 Zeichen  
- String-Wert: max. 4000 Bytes
- EEPROM-Gesamt: 4KB (begrenzt, daher werden Sensordaten noch in JSON gespeichert)

## Zukünftige Erweiterungen

In zukünftigen Versionen könnten folgende Verbesserungen implementiert werden:
- Migration der Sensor-Grundeinstellungen zu Preferences
- Verwendung eines separaten Flash-Bereichs für größere Sensor-Daten
- Kompression von Sensor-Konfigurationen für EEPROM-Speicherung
