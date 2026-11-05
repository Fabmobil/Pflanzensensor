/**
 * @file uploads.cpp
 */
#include "web/uploads.h"

#include <stdlib.h>

#include "hal/system.h"
#include "services/log.h"

namespace ps {
namespace web {

bool OtaUpload::uploadStart(const hal::Request& req, const char* field, const char* filename) {
  accepted_ = r_.authorized(req);
  if (!accepted_) {
    LOGW(Mod::Ota, "Upload ohne Anmeldung verworfen");
    return false;
  }
  char buf[40] = "";
  uint32_t size = 0;
  if (req.arg("size", buf, sizeof(buf)))
    size = static_cast<uint32_t>(strtoul(buf, nullptr, 10));
  char md5[40] = "";
  req.arg("md5", md5, sizeof(md5));
  accepted_ = c_.ota.start(size, md5, hal::now()) == Err::None;
  LOGI(Mod::Ota, "Datei: %s", filename ? filename : "?");
  return accepted_;
}

void OtaUpload::uploadData(const uint8_t* data, size_t len) {
  if (accepted_)
    c_.ota.data(data, len, hal::now());
}

void OtaUpload::uploadEnd(uint32_t) {
  if (accepted_)
    c_.ota.finish(hal::now());
  accepted_ = false;
}

void OtaUpload::uploadAborted() {
  if (accepted_)
    c_.ota.abort("Upload abgebrochen");
  accepted_ = false;
}

bool ImportUpload::uploadStart(const hal::Request& req, const char*, const char*) {
  if (file_)
    c_.fs.close(file_);
  file_ = nullptr;
  size_ = 0;
  tooBig_ = false;
  if (!r_.authorized(req))
    return false;
  file_ = c_.fs.open(ConfigService::kImportPath, hal::OpenMode::Write);
  return file_ != nullptr;
}

void ImportUpload::uploadData(const uint8_t* data, size_t len) {
  if (!file_)
    return;
  size_ += static_cast<uint32_t>(len);
  if (size_ > kMaxSize) {
    tooBig_ = true;
    c_.fs.close(file_);
    file_ = nullptr;
    c_.fs.remove(ConfigService::kImportPath);
    return;
  }
  file_->writeBytes(data, len);
}

void ImportUpload::uploadEnd(uint32_t) {
  if (file_)
    c_.fs.close(file_);
  file_ = nullptr;
  if (tooBig_)
    LOGW(Mod::Config, "Importdatei zu groß (> 16 KB) - verworfen");
}

void ImportUpload::uploadAborted() {
  if (file_)
    c_.fs.close(file_);
  file_ = nullptr;
  c_.fs.remove(ConfigService::kImportPath);
}

} // namespace web
} // namespace ps
