/**
 * @file storage.cpp
 */
#include "services/storage.h"

#include <stdio.h>
#include <string.h>

#include "core/chronik/segments.h"

#include "services/log.h"

namespace ps {
namespace storage {

namespace {
void tmpName(const char* path, char* out, size_t cap) { snprintf(out, cap, "%s.tmp", path); }

/// Fehlt path, aber path.tmp existiert: übernehmen
void recoverTmp(hal::Fs& fs, const char* path, bool (*valid)(hal::Fs&, const char*)) {
  if (fs.exists(path))
    return;
  char tmp[40];
  tmpName(path, tmp, sizeof(tmp));
  if (!fs.exists(tmp))
    return;
  if (valid && !valid(fs, tmp)) {
    LOGW(Mod::Fs, "Unvollständige %s verworfen", tmp);
    fs.remove(tmp);
    return;
  }
  LOGW(Mod::Fs, "%s aus Temp-Datei wiederhergestellt", path);
  fs.rename(tmp, path);
}

bool jsonValid(hal::Fs& fs, const char* path) {
  hal::FileGuard f(fs, fs.open(path, hal::OpenMode::Read));
  if (!f)
    return false;
  JsonDocument doc;
  FileReader r(*f.get());
  return deserializeJson(doc, r) == DeserializationError::Ok;
}
} // namespace

bool writeAtomic(hal::Fs& fs, const char* path, WriteFn fn, void* ctx) {
  char tmp[40];
  tmpName(path, tmp, sizeof(tmp));
  bool ok;
  {
    hal::FileGuard f(fs, fs.open(tmp, hal::OpenMode::Write));
    if (!f) {
      LOGE(Mod::Fs, "%s nicht anlegbar", tmp);
      return false;
    }
    ok = fn(*f.get(), ctx) && !f->failed();
  }
  if (!ok) {
    LOGE(Mod::Fs, "Schreiben von %s fehlgeschlagen", path);
    fs.remove(tmp);
    return false;
  }
  if (!fs.rename(tmp, path)) {
    LOGE(Mod::Fs, "Umbenennen nach %s fehlgeschlagen", path);
    return false;
  }
  return true;
}

bool readJson(hal::Fs& fs, const char* path, JsonDocument& doc) {
  recoverTmp(fs, path, jsonValid);
  hal::FileGuard f(fs, fs.open(path, hal::OpenMode::Read));
  if (!f)
    return false;
  FileReader r(*f.get());
  const DeserializationError e = deserializeJson(doc, r);
  if (e != DeserializationError::Ok) {
    LOGE(Mod::Config, "%s nicht lesbar (%s)", path, e.c_str());
    return false;
  }
  return true;
}

bool readBinary(hal::Fs& fs, const char* path, uint8_t* buf, size_t cap, size_t& len) {
  recoverTmp(fs, path, nullptr);
  hal::FileGuard f(fs, fs.open(path, hal::OpenMode::Read));
  if (!f)
    return false;
  len = f->read(buf, cap);
  return true;
}

namespace {
/// Alle Dateien der Firmware im Wurzelverzeichnis (ohne Chronik-Segmente).
/// Neue Dateien hier eintragen, sonst löscht removeForeign() sie beim Start.
const char* const kOwnFiles[] = {"device.cfg", "sensors.cfg", "secrets.cfg", "state.bin",
                                 "mail.tpl",   "boot.cnt",    "log.txt",     "import.tmp"};

bool equalsOwn(const char* p, size_t len) {
  for (const char* own : kOwnFiles)
    if (strlen(own) == len && strncmp(p, own, len) == 0)
      return true;
  return false;
}
} // namespace

bool isOwnFile(const char* name) {
  if (!name)
    return false;
  const char* p = name[0] == '/' ? name + 1 : name;
  if (chronik::indexFromName(p) >= 0)
    return true;
  const size_t len = strlen(p);
  if (equalsOwn(p, len))
    return true;
  // atomares Schreiben: "<eigene Datei>.tmp"
  return len > 4 && strcmp(p + len - 4, ".tmp") == 0 && equalsOwn(p, len - 4);
}

CleanupResult removeForeign(hal::Fs& fs) {
  CleanupResult r;
  hal::FsInfo before;
  const bool haveBefore = fs.info(before);
  // Je Runde den ersten (noch nicht gescheiterten) Fremdeintrag suchen und
  // nach dem Auflisten löschen - nie während der Iteration.
  for (uint8_t round = 0; round < 64; round++) {
    struct Ctx {
      uint16_t skip;
      uint16_t seen;
      bool found;
      bool dir;
      char name[48];
    } ctx{r.failed, 0, false, false, ""};
    fs.listRoot(
        [](void* c, const char* n, bool dir) {
          Ctx& x = *static_cast<Ctx*>(c);
          if (x.found || isOwnFile(n))
            return;
          if (x.seen++ < x.skip)
            return;
          snprintf(x.name, sizeof(x.name), "%s", n);
          x.dir = dir;
          x.found = true;
        },
        &ctx);
    if (!ctx.found)
      break;
    if (fs.removeTree(ctx.name)) {
      r.removed++;
      LOGI(Mod::Fs, "Altlast entfernt: %s%s", ctx.name, ctx.dir ? "/" : "");
    } else {
      r.failed++;
      LOGW(Mod::Fs, "Altlast nicht löschbar: %s", ctx.name);
    }
  }
  hal::FsInfo after;
  if (r.removed && haveBefore && fs.info(after) && after.used < before.used)
    r.freedBytes = before.used - after.used;
  return r;
}

} // namespace storage
} // namespace ps
