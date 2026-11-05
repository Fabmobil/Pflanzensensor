/**
 * @file http_server.h
 * @brief HTTP-Anfragen und -Antworten (ESP8266WebServer/WebServer dahinter)
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/json_writer.h"

namespace ps {
namespace hal {

enum class HttpMethod : uint8_t { Get, Post, Put, Patch, Delete, Options, Other };

class Request {
public:
  virtual HttpMethod method() const = 0;
  /// Pfad ohne Query
  virtual const char* path() const = 0;
  /// Query- oder Formularparameter; false, wenn nicht vorhanden
  virtual bool arg(const char* name, char* out, size_t cap) const = 0;
  virtual bool hasArg(const char* name) const = 0;
  /// Rumpf (JSON bei PATCH/PUT/POST); nullptr wenn keiner
  virtual const char* body() const = 0;
  virtual size_t bodyLength() const = 0;
  virtual bool header(const char* name, char* out, size_t cap) const = 0;

protected:
  ~Request() = default;
};

class Response : public Sink {
public:
  virtual void header(const char* name, const char* value) = 0;
  /// Antwort beginnen. contentLength < 0: chunked
  virtual void begin(int code, const char* contentType, int32_t contentLength) = 0;
  /// Bytes aus dem Flash (Assets)
  virtual void writeFlash(const uint8_t* data, size_t len) = 0;
  /// Antwort abschließen (Puffer leeren, chunked beenden)
  virtual void end() = 0;
  virtual bool started() const = 0;

protected:
  ~Response() = default;
};

/// Datei-Upload (multipart) eines Formularfelds
class UploadHandler {
public:
  /// Upload beginnt; false lehnt ab (z.B. ohne Anmeldung), weitere Daten werden verworfen
  virtual bool uploadStart(const Request& req, const char* field, const char* filename) = 0;
  virtual void uploadData(const uint8_t* data, size_t len) = 0;
  virtual void uploadEnd(uint32_t total) = 0;
  virtual void uploadAborted() = 0;

protected:
  ~UploadHandler() = default;
};

using RequestFn = void (*)(Request& req, Response& res, void* ctx);

class HttpServer {
public:
  /// Alle Anfragen gehen an dispatch (statische Routentabelle im Web-Teil)
  virtual void begin(uint16_t port, RequestFn dispatch, void* ctx) = 0;
  /// Upload-Route (POST) anmelden; einmalig vor begin()
  virtual void addUpload(const char* path, UploadHandler* handler) = 0;
  /// Genau eine Anfrage bearbeiten
  virtual void handle() = 0;
  virtual void stop() = 0;

protected:
  ~HttpServer() = default;
};

} // namespace hal
} // namespace ps
