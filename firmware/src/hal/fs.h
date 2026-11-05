/**
 * @file fs.h
 * @brief Dateisystem (LittleFS), nur flaches Wurzelverzeichnis
 * @details Dateien kommen aus einem festen Pool im Plattformteil; open()
 *          liefert nullptr, wenn keine frei ist. Immer mit FileGuard schließen.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/json_writer.h"

namespace ps {
namespace hal {

enum class OpenMode : uint8_t { Read, Write, Append, ReadWrite };

class File : public Sink {
public:
  virtual size_t read(uint8_t* buf, size_t len) = 0;
  virtual size_t writeBytes(const uint8_t* buf, size_t len) = 0;
  virtual bool seek(uint32_t pos) = 0;
  virtual uint32_t position() = 0;
  virtual uint32_t size() = 0;
  virtual bool truncate(uint32_t len) = 0;
  /// Sink-Schnittstelle (für JsonWriter); merkt Schreibfehler
  void write(const char* data, size_t len) override {
    if (writeBytes(reinterpret_cast<const uint8_t*>(data), len) != len)
      failed_ = true;
  }
  using Sink::write;
  bool failed() const { return failed_; }
  void resetFailed() { failed_ = false; }

protected:
  ~File() = default;
  bool failed_ = false;
};

struct FsInfo {
  uint32_t total = 0;
  uint32_t used = 0;
};

using ListCallback = void (*)(void* ctx, const char* name, uint32_t size);
using EntryCallback = void (*)(void* ctx, const char* name, bool directory);

class Fs {
public:
  virtual bool mount() = 0;
  virtual bool format() = 0;
  virtual File* open(const char* path, OpenMode mode) = 0;
  virtual void close(File* f) = 0;
  virtual bool exists(const char* path) = 0;
  virtual bool remove(const char* path) = 0;
  /// Ersetzt ein vorhandenes Ziel atomar
  virtual bool rename(const char* from, const char* to) = 0;
  virtual bool info(FsInfo& out) = 0;
  /// Alle Dateien im Wurzelverzeichnis
  virtual void list(ListCallback cb, void* ctx) = 0;
  /// Alle Einträge im Wurzelverzeichnis, auch Verzeichnisse
  virtual void listRoot(EntryCallback cb, void* ctx) = 0;
  /// Datei oder Verzeichnis samt Inhalt löschen
  virtual bool removeTree(const char* path) = 0;

protected:
  ~Fs() = default;
};

/// Schließt eine Datei beim Verlassen des Blocks
class FileGuard {
public:
  FileGuard(Fs& fs, File* f) : fs_(fs), f_(f) {}
  ~FileGuard() { reset(); }
  FileGuard(const FileGuard&) = delete;
  FileGuard& operator=(const FileGuard&) = delete;
  File* get() const { return f_; }
  File* operator->() const { return f_; }
  explicit operator bool() const { return f_ != nullptr; }
  void reset() {
    if (f_)
      fs_.close(f_);
    f_ = nullptr;
  }

private:
  Fs& fs_;
  File* f_;
};

} // namespace hal
} // namespace ps
