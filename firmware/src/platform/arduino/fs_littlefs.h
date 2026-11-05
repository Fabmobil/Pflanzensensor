/**
 * @file fs_littlefs.h
 * @brief hal::Fs auf LittleFS (ESP8266 und ESP32)
 * @details Dateien aus einem festen Pool von 4 Plätzen.
 */
#pragma once

#if defined(ARDUINO)

#include <FS.h>

#include "hal/fs.h"

namespace ps {
namespace platform {

class LittleFs : public hal::Fs {
public:
  bool mount() override;
  bool format() override;
  hal::File* open(const char* path, hal::OpenMode mode) override;
  void close(hal::File* f) override;
  bool exists(const char* path) override;
  bool remove(const char* path) override;
  bool rename(const char* from, const char* to) override;
  bool info(hal::FsInfo& out) override;
  void list(hal::ListCallback cb, void* ctx) override;
  void listRoot(hal::EntryCallback cb, void* ctx) override;
  bool removeTree(const char* path) override;

  class Handle : public hal::File {
  public:
    size_t read(uint8_t* buf, size_t len) override { return f.read(buf, len); }
    size_t writeBytes(const uint8_t* buf, size_t len) override { return f.write(buf, len); }
    bool seek(uint32_t pos) override { return f.seek(pos); }
    uint32_t position() override { return static_cast<uint32_t>(f.position()); }
    uint32_t size() override { return static_cast<uint32_t>(f.size()); }
    bool truncate(uint32_t len) override;
    fs::File f;
    bool used = false;
    char path[32] = "";
  };

private:
  Handle slots_[4];
  bool mounted_ = false;
};

} // namespace platform
} // namespace ps

#endif
