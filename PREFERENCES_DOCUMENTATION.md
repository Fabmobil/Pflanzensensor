# Preferences-basierte Konfigurationsverwaltung

## Überblick

Das Pflanzensensor-Projekt verwendet jetzt die Preferences-Bibliothek zur Speicherung der Konfiguration auf LittleFS. Dies bietet eine robustere und effizientere Alternative zu JSON-Dateien.

## Namespace-Organisation

Die Konfiguration ist in logische Namespaces unterteilt:

### 1. General Settings (`general`)
- Gerätename (`device_name`)
- Admin-Passwort (`admin_pwd`)
- MD5-Verifizierung (`md5_verify`)
- Datei-Logging (`file_log`)
- Flower-Status-Sensor (`flower_sens`)

### 2. WiFi Settings (`wifi`)
- SSID 1-3 (`ssid1`, `ssid2`, `ssid3`)
- Passwörter 1-3 (`pwd1`, `pwd2`, `pwd3`)

### 3. Display Settings (`display`)
- IP-Bildschirm anzeigen (`show_ip`)
- Uhr anzeigen (`show_clock`)
- Blumenbild anzeigen (`show_flower`)
- Fabmobil-Bild anzeigen (`show_fabmobil`)
- Bildschirmdauer (`screen_dur`)
- Uhrformat (`clock_fmt`)

### 4. Log Settings (`log`)
- Log-Level (`level`)
- Datei-Logging aktiviert (`file_enabled`)

### 5. LED Traffic Light Settings (`led_traf`)
- Modus (`mode`): 0 = aus, 1 = alle Messungen, 2 = einzelne Messung
- Ausgewählte Messung (`sel_meas`)

### 6. Debug Settings (`debug`)
- RAM-Debug (`ram`)
- Messzyklus-Debug (`meas_cycle`)
- Sensor-Debug (`sensor`)
- Display-Debug (`display`)
- WebSocket-Debug (`websocket`)

### 7. Sensor Settings (dynamisch, `s_<sensorId>`)
Jeder Sensor hat seinen eigenen Namespace mit:
- Name (`name`)
- Messintervall (`meas_int`)
- Pro Messung (Index 0-N):
  - Aktiviert (`m<N>_en`)
  - Minimum (`m<N>_min`)
  - Maximum (`m<N>_max`)
  - Schwellenwerte (`m<N>_yl`, `m<N>_gl`, `m<N>_gh`, `m<N>_yh`)
  - Name (`m<N>_nm`)

## Boot-Prozess

1. Das System prüft beim Start, ob Preferences vorhanden sind
2. Wenn nicht vorhanden:
   - Werden Preferences mit Standardwerten aus `config_example.h` initialisiert
   - Falls eine `config.json` existiert, wird diese zu Preferences migriert
3. Wenn vorhanden:
   - Werden die gespeicherten Werte geladen

## Migration von JSON zu Preferences

Die Migration erfolgt automatisch beim ersten Start:

1. Beim Laden wird zuerst Preferences geprüft
2. Wenn nicht vorhanden, wird nach `config.json` gesucht
3. JSON-Werte werden eingelesen und in Preferences gespeichert
4. Die JSON-Datei wird zu `.bak` umbenannt als Backup
5. Zukünftige Ladevorgänge verwenden Preferences

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

## Vorteile gegenüber JSON

1. **Effizienz**: Schnellerer Zugriff auf einzelne Werte
2. **Robustheit**: Weniger anfällig für Korruption
3. **Speicher**: Geringerer RAM-Verbrauch beim Lesen/Schreiben
4. **Atomare Updates**: Einzelne Werte können ohne Laden der gesamten Konfiguration aktualisiert werden

## Rückwärtskompatibilität

Das System unterstützt weiterhin:
- Automatische Migration von bestehenden JSON-Konfigurationen
- Fallback auf JSON, wenn Preferences-Initialisierung fehlschlägt
- Backup der alten JSON-Dateien bei Migration

## Debugging

Alle Preferences-Operationen werden mit deutschen Logger-Meldungen protokolliert:
- Erfolgreiche Lade-/Speichervorgänge
- Migrations-Status
- Fehler mit detaillierten Meldungen

## Datenspeicher-Limits

- Namespace-Name: max. 15 Zeichen
- Key-Name: max. 15 Zeichen  
- String-Wert: max. 4000 Bytes
- Gesamt-Speicher: abhängig vom Flash-Speicher (typisch mehrere MB)
