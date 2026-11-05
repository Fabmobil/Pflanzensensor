/**
 * @file fs_littlefs.cpp
 */
#if defined(ARDUINO)

#include "platform/arduino/fs_littlefs.h"

#include <LittleFS.h>

namespace ps {
namespace platform {

namespace {
const char* modeString(hal::OpenMode m) {
  switch (m) {
  case hal::OpenMode::Write:
    return "w";
  case hal::OpenMode::Append:
    return "a";
  case hal::OpenMode::ReadWrite:
    return "r+";
  default:
    return "r";
  }
}
} // namespace

bool LittleFs::mount() {
#if defined(ARDUINO_ARCH_ESP32)
  mounted_ = LittleFS.begin(false);
#else
  mounted_ = LittleFS.begin();
#endif
  return mounted_;
}

bool LittleFs::format() { return LittleFS.format(); }

hal::File* LittleFs::open(const char* path, hal::OpenMode mode) {
  if (!mounted_)
    return nullptr;
  if (mode == hal::OpenMode::Read || mode == hal::OpenMode::ReadWrite) {
    if (!LittleFS.exists(path))
      return nullptr;
  }
  for (auto& s : slots_) {
    if (s.used)
      continue;
    s.f = LittleFS.open(path, modeString(mode));
    if (!s.f)
      return nullptr;
    s.used = true;
    s.resetFailed();
    strncpy(s.path, path, sizeof(s.path) - 1);
    s.path[sizeof(s.path) - 1] = '\0';
    return &s;
  }
  return nullptr;
}

void LittleFs::close(hal::File* f) {
  for (auto& s : slots_) {
    if (&s != f)
      continue;
    s.f.close();
    s.used = false;
    return;
  }
}

bool LittleFs::Handle::truncate(uint32_t len) {
#if defined(ARDUINO_ARCH_ESP8266)
  return f.truncate(len);
#else
  // ESP32: kein truncate() in der File-API - Präfix umkopieren
  char tmp[40];
  snprintf(tmp, sizeof(tmp), "%s.trn", path);
  fs::File out = LittleFS.open(tmp, "w");
  if (!out)
    return false;
  f.seek(0);
  uint8_t buf[128];
  uint32_t left = len;
  while (left > 0) {
    const size_t n = f.read(buf, left < sizeof(buf) ? left : sizeof(buf));
    if (!n)
      break;
    out.write(buf, n);
    left -= static_cast<uint32_t>(n);
  }
  out.close();
  f.close();
  const bool ok = LittleFS.rename(tmp, path);
  f = LittleFS.open(path, "r+");
  return ok;
#endif
}

bool LittleFs::exists(const char* path) { return mounted_ && LittleFS.exists(path); }

bool LittleFs::remove(const char* path) {
  if (!mounted_ || !LittleFS.exists(path))
    return false;
  return LittleFS.remove(path);
}

bool LittleFs::rename(const char* from, const char* to) {
  if (!mounted_)
    return false;
  if (LittleFS.rename(from, to))
    return true;
  // Rückfall, falls das Ziel nicht ersetzt werden kann
  if (LittleFS.exists(to) && LittleFS.remove(to))
    return LittleFS.rename(from, to);
  return false;
}

bool LittleFs::info(hal::FsInfo& out) {
  if (!mounted_)
    return false;
#if defined(ARDUINO_ARCH_ESP8266)
  FSInfo i;
  if (!LittleFS.info(i))
    return false;
  out.total = static_cast<uint32_t>(i.totalBytes);
  out.used = static_cast<uint32_t>(i.usedBytes);
#else
  out.total = static_cast<uint32_t>(LittleFS.totalBytes());
  out.used = static_cast<uint32_t>(LittleFS.usedBytes());
#endif
  return true;
}

void LittleFs::list(hal::ListCallback cb, void* ctx) {
  if (!mounted_)
    return;
#if defined(ARDUINO_ARCH_ESP8266)
  Dir d = LittleFS.openDir("/");
  while (d.next()) {
    if (!d.isFile())
      continue;
    char name[40];
    snprintf(name, sizeof(name), "/%s", d.fileName().c_str());
    cb(ctx, name, static_cast<uint32_t>(d.fileSize()));
  }
#else
  fs::File root = LittleFS.open("/");
  if (!root)
    return;
  for (fs::File f = root.openNextFile(); f; f = root.openNextFile()) {
    if (f.isDirectory())
      continue;
    char name[40];
    const char* n = f.name();
    snprintf(name, sizeof(name), "%s%s", n[0] == '/' ? "" : "/", n);
    cb(ctx, name, static_cast<uint32_t>(f.size()));
  }
#endif
}

void LittleFs::listRoot(hal::EntryCallback cb, void* ctx) {
  if (!mounted_)
    return;
  char name[48];
#if defined(ARDUINO_ARCH_ESP8266)
  Dir d = LittleFS.openDir("/");
  while (d.next()) {
    snprintf(name, sizeof(name), "/%s", d.fileName().c_str());
    cb(ctx, name, d.isDirectory());
  }
#else
  fs::File root = LittleFS.open("/");
  if (!root)
    return;
  for (fs::File f = root.openNextFile(); f; f = root.openNextFile()) {
    const char* n = f.name();
    snprintf(name, sizeof(name), "%s%s", n[0] == '/' ? "" : "/", n);
    cb(ctx, name, f.isDirectory());
  }
#endif
}

namespace {
constexpr uint8_t kMaxTreeDepth = 6;

/// Erster Eintrag eines Verzeichnisses als voller Pfad; false, wenn leer
bool firstChild(const char* dir, char* out, size_t cap) {
#if defined(ARDUINO_ARCH_ESP8266)
  Dir d = LittleFS.openDir(dir);
  if (!d.next())
    return false;
  snprintf(out, cap, "%s/%s", dir, d.fileName().c_str());
#else
  fs::File root = LittleFS.open(dir);
  if (!root || !root.isDirectory())
    return false;
  fs::File e = root.openNextFile();
  if (!e)
    return false;
  const char* n = e.name();
  const char* base = strrchr(n, '/');
  snprintf(out, cap, "%s/%s", dir, base ? base + 1 : n);
#endif
  return true;
}

bool removeRecursive(const char* path, uint8_t depth) {
  if (LittleFS.remove(path))
    return true; // Datei (oder leeres Verzeichnis)
  if (!LittleFS.exists(path))
    return true;
  if (depth >= kMaxTreeDepth)
    return false;
  // Immer den ersten Eintrag löschen und neu auflisten: Löschen während
  // der Iteration ist bei LittleFS nicht zuverlässig.
  for (uint16_t guard = 0; guard < 1024; guard++) {
    char child[96];
    if (!firstChild(path, child, sizeof(child)))
      break;
    if (!removeRecursive(child, static_cast<uint8_t>(depth + 1)))
      return false;
    yield();
  }
  LittleFS.rmdir(path);
  return !LittleFS.exists(path);
}
} // namespace

bool LittleFs::removeTree(const char* path) {
  if (!mounted_ || !path || !*path || strcmp(path, "/") == 0)
    return false;
  return removeRecursive(path, 0);
}

} // namespace platform
} // namespace ps

#endif
