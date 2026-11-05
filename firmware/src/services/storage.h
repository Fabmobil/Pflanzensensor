/**
 * @file storage.h
 * @brief Atomares Schreiben und robustes Lesen von Dateien
 * @details Schreiben: in "<name>.tmp", schließen, rename (LittleFS ersetzt
 *          atomar). Lesen: fehlt "<name>", aber "<name>.tmp" ist gültig, wird
 *          sie übernommen (Befund B-16).
 */
#pragma once

#include <ArduinoJson.h>

#include "hal/fs.h"

namespace ps {
namespace storage {

using WriteFn = bool (*)(hal::File& f, void* ctx);

/// Schreibt atomar über eine Temp-Datei
bool writeAtomic(hal::Fs& fs, const char* path, WriteFn fn, void* ctx);

/// Adapter, damit ArduinoJson direkt aus einer Datei liest
class FileReader {
public:
  explicit FileReader(hal::File& f) : f_(f) {}
  int read() {
    uint8_t c;
    return f_.read(&c, 1) == 1 ? c : -1;
  }
  size_t readBytes(char* buf, size_t len) { return f_.read(reinterpret_cast<uint8_t*>(buf), len); }

private:
  hal::File& f_;
};

/**
 * @brief JSON-Datei lesen (mit .tmp-Wiederherstellung)
 * @return false, wenn keine lesbare Datei existiert
 */
bool readJson(hal::Fs& fs, const char* path, JsonDocument& doc);

/// Binärdatei vollständig lesen (mit .tmp-Wiederherstellung)
bool readBinary(hal::Fs& fs, const char* path, uint8_t* buf, size_t cap, size_t& len);

/// Gehört die Datei im Wurzelverzeichnis zu Firmware 3 (inkl. .tmp, Chronik)?
bool isOwnFile(const char* name);

struct CleanupResult {
  uint16_t removed = 0;
  uint16_t failed = 0;
  uint32_t freedBytes = 0;
};

/**
 * @brief Fremde Dateien und Verzeichnisse löschen (Altlasten von Firmware 2.x)
 * @details Die alte Firmware hat Web-Dateien, /nvs, /config usw. im
 *          LittleFS abgelegt. Eine Migration gibt es nicht (refactor.md E2);
 *          ohne Aufräumen bleiben ~650 KB belegt und die Chronik bekommt
 *          kaum Platz. Läuft bei jedem Start, ist ohne Fremddateien billig.
 */
CleanupResult removeForeign(hal::Fs& fs);

} // namespace storage
} // namespace ps
