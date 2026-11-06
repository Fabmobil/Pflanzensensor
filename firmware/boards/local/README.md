# Eigene Geräte (nicht versioniert)

Alles unter `boards/local/` außer dieser Datei ignoriert Git. Jedes Gerät
bekommt ein Verzeichnis mit drei Dateien:

```
boards/local/<gerät>/
  platformio.ini   Env [env:<gerät>], wird über extra_configs eingebunden
  profile.h        Board-Profil: Plattform, Pins, Features (wie boards/pcb_v1.h)
  defaults.h       Standardwerte: Name, WLAN, Admin-Passwort, Kanäle
```

Bauen und flashen:

```sh
cd firmware
pio run -e <gerät> -t upload
```

## platformio.ini

```ini
[env:<gerät>]
extends = esp8266            ; oder esp32
build_flags =
	${esp8266.build_flags}
	-D BOARD_PROFILE=\"local/<gerät>/profile.h\"
	-D PS_LOCAL_DEFAULTS=\"local/<gerät>/defaults.h\"
```

## profile.h

Wie `boards/pcb_v1.h`: `namespace board_profile` mit `kPlatform`, `kName`
(höchstens 16 Zeichen), `kPlatformName`, `kPins`, `kFeatures`, `kStaticIp`.
`profile_check.h` prüft die Pins beim Bauen.

## defaults.h

Wird nur in `src/core/config/defaults.cpp` eingebunden, nach den
Standardwerten. Er muss Folgendes liefern:

```cpp
#pragma once
#include "core/config/device_config.h"
#include "core/config/schema.h"
#include "core/config/sensor_config.h"

namespace local_defaults {
constexpr const char* kAdminPassword = "…";
inline void device(ps::DeviceConfig& c) { /* Name, WLAN-SSIDs, LED, Log … */ }
inline void secrets(ps::Secrets& s) { /* WLAN-Passwörter */ }
inline void sensors(ps::SensorsConfig& c) { /* Kanalnamen, Grenzen … */ }
}
```

Gemeinsame Werte aller Geräte (WLAN, Admin-Passwort, Hilfsfunktionen wie
`setChannel()`) stehen in `boards/local/common.h`; die `defaults.h` der
Geräte binden sie mit `#include "local/common.h"` ein.

Kanäle findet man über ihren Schlüssel: `ps::schema::slotOfKey("DHT_0")`.
Die Schlüssel zählen ab 0 (`DHT_0` Temperatur, `DHT_1` Feuchte, `MHZ19_0`,
`ANALOG_0` …).

## Wann gelten die Werte?

- Die Standardwerte gelten nur, solange auf dem Gerät noch keine
  Konfiguration liegt: beim ersten Start, nach dem Werksreset und nach dem
  Wechsel von Firmware 2.x. Danach gilt, was auf dem Gerät gespeichert ist.
- Die Hardware kommt bei lokalen Builds immer aus `profile.h`. Ein
  Provisionierungsblock, den der Webflasher früher geschrieben hat, wird
  für die Hardware ignoriert (Log: „Hardware aus dem Webflasher ignoriert“).
  Bei den offiziellen Envs (`esp8266_pcb`, `esp32_devkit`) hat der Block
  weiterhin Vorrang.
