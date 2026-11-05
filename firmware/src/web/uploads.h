/**
 * @file uploads.h
 * @brief Datei-Uploads: Firmware (OTA) und Konfigurations-Import
 * @details Die Anmeldung wird schon beim Upload-Beginn geprüft; ohne sie
 *          werden die Daten verworfen.
 */
#pragma once

#include "hal/http_server.h"
#include "web/context.h"
#include "web/router.h"

namespace ps {
namespace web {

class OtaUpload : public hal::UploadHandler {
public:
  OtaUpload(WebContext& c, Router& r) : c_(c), r_(r) {}
  bool uploadStart(const hal::Request& req, const char* field, const char* filename) override;
  void uploadData(const uint8_t* data, size_t len) override;
  void uploadEnd(uint32_t total) override;
  void uploadAborted() override;

private:
  WebContext& c_;
  Router& r_;
  bool accepted_ = false;
};

class ImportUpload : public hal::UploadHandler {
public:
  static constexpr uint32_t kMaxSize = 16384;
  ImportUpload(WebContext& c, Router& r) : c_(c), r_(r) {}
  bool uploadStart(const hal::Request& req, const char* field, const char* filename) override;
  void uploadData(const uint8_t* data, size_t len) override;
  void uploadEnd(uint32_t total) override;
  void uploadAborted() override;

private:
  WebContext& c_;
  Router& r_;
  hal::File* file_ = nullptr;
  uint32_t size_ = 0;
  bool tooBig_ = false;
};

} // namespace web
} // namespace ps
