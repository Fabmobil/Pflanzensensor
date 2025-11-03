# Preferences-basierte Konfigurationsverwaltung

## Überblick

Das Pflanzensensor-Projekt verwendet die Preferences-Bibliothek zur Speicherung der Konfiguration im EEPROM. Dies bietet eine robustere und effizientere Alternative zu JSON-Dateien und **überlebt Filesystem-Flashes**, da die Preferences im EEPROM-Bereich (0x405FB000) gespeichert werden, der getrennt vom LittleFS-Bereich (0x40512000-0x405FA000) ist.

## Namespace-Organisation

Die Konfiguration ist in logische Namespaces unterteilt:

### Derzeit in Preferences gespeichert

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

### Noch in JSON gespeichert (sensors.json)

#### 7. Sensor Settings
Die folgenden Sensor-Einstellungen werden derzeit noch in `sensors.json` gespeichert:
- Sensor-Name und Messintervall
- Pro Messung:
  - Aktiviert/Deaktiviert
  - Absolute Min/Max-Werte
  - Schwellenwerte (yellowLow, greenLow, greenHigh, yellowHigh)
  - **Analog-spezifische Einstellungen:**
    - Raw Min/Max-Werte (`absoluteRawMin`, `absoluteRawMax`)
    - Kalibrierungsmodus (`calibrationMode`)
    - Autocal-Dauer (`autocalDuration`)
    - Autocal-Status (`min_value`, `max_value`, `last_update_time`)
    - Invertierung (`inverted`)

**Hinweis:** Die Sensor-Einstellungen werden in einer zukünftigen Version zu Preferences migriert.

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
4. Sensor-Einstellungen werden aus `sensors.json` geladen (falls vorhanden)

**Keine automatische Migration:** Es gibt keine automatische Migration von JSON zu Preferences mehr. Die Preferences werden beim ersten Boot mit Standardwerten initialisiert.

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
