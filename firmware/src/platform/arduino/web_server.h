/**
 * @file web_server.h
 * @brief hal::HttpServer auf ESP8266WebServer (ESP32: WebServer, gleiche API)
 * @details Eine Anfrage pro Schleife. Alle Routen laufen über onNotFound in
 *          die statische Tabelle; nur Upload-Routen werden mit on()
 *          angemeldet, weil die Bibliothek Uploads sonst nicht zustellt.
 */
#pragma once

#if defined(ARDUINO)

#include "hal/http_server.h"

namespace ps {
namespace platform {

class WebServer : public hal::HttpServer {
public:
  void begin(uint16_t port, hal::RequestFn dispatch, void* ctx) override;
  void addUpload(const char* path, hal::UploadHandler* handler) override;
  void handle() override;
  void stop() override;

private:
  void serve();
  void upload(hal::UploadHandler* h);

  hal::RequestFn dispatch_ = nullptr;
  void* ctx_ = nullptr;
  struct Up {
    const char* path;
    hal::UploadHandler* handler;
  } uploads_[2] = {};
  uint8_t uploadCount_ = 0;
  bool uploadAccepted_ = false;
};

} // namespace platform
} // namespace ps

#endif
