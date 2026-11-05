/**
 * @file web_server.cpp
 */
#if defined(ARDUINO)

#include "platform/arduino/web_server.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
using ServerImpl = ::WebServer;
#endif

namespace ps {
namespace platform {

namespace {

#if defined(ARDUINO_ARCH_ESP8266)
/**
 * ESP8266WebServer mit nicht blockierendem Vorfilter.
 *
 * Die Bibliothek liest Kopfzeilen mit readStringUntil() und den Rumpf mit
 * readBytesWithTimeout() - beides wartet bis zu 5 s je Aufruf, wenn ein
 * Client Daten nur teilweise schickt (langsame Verbindung, abgebrochene
 * Anfrage). Die ganze Hauptschleife stand dann bis zu 10 s (Abnahme).
 * Deshalb wird eine Anfrage erst an die Bibliothek gegeben, wenn Kopf und
 * kleiner Rumpf vollständig im Empfangspuffer liegen. Uploads (multipart)
 * laufen weiter blockierend - dabei pausieren die Sensoren ohnehin.
 *
 * Außerdem behält ein String seine Kapazität: eine 6000 Zeichen lange URL
 * oder ein 3-KB-Authorization-Kopf belegten danach dauerhaft Heap. Nach
 * jeder Anfrage werden übergroße Puffer freigegeben.
 */
class GuardedServer : public ESP8266WebServer {
public:
  using ESP8266WebServer::ESP8266WebServer;

  void loop() {
    if (_currentStatus == HC_NONE) {
      WiFiClient c = _server.accept();
      if (!c)
        return;
      _currentClient = c;
      _currentStatus = HC_WAIT_READ;
      _statusChange = millis();
    }
    const bool waiting = _currentStatus == HC_WAIT_READ || _currentStatus == HC_WAIT_CLOSE;
    if (waiting && _currentClient.available()) {
      switch (check()) {
      case Check::Incomplete: {
        // Warten andere Clients, bekommt ein unvollständiger nur 500 ms
        const unsigned long waited = millis() - _statusChange;
        const bool others = _server.hasClientData() || _server.hasMaxPendingClients();
        if (waited > HTTP_MAX_DATA_WAIT || (others && waited > kIncompleteWithOthersMs))
          drop(nullptr);
        return;
      }
      case Check::TooLarge:
        drop(PSTR("413 Payload Too Large"));
        return;
      case Check::UriTooLong:
        drop(PSTR("414 URI Too Long"));
        return;
      case Check::Bad:
        drop(PSTR("400 Bad Request"));
        return;
      case Check::Ready:
        break;
      }
    }
    handleClient();
    if (_currentStatus != HC_WAIT_READ)
      trimStrings();
  }

private:
  enum class Check : uint8_t { Ready, Incomplete, TooLarge, UriTooLong, Bad };
  /// JSON-Rümpfe der API sind klein; größere Rümpfe ohne multipart gibt es nicht
  static constexpr long kMaxPlainBody = 8192;
  static constexpr size_t kMaxRequestLine = 512;
  static constexpr unsigned kKeepStringMax = 128;
  static constexpr unsigned long kIncompleteWithOthersMs = 500;

  /// needle (Flash, klein geschrieben) in [p, p+n) ohne Groß/Klein; nullptr wenn nicht da
  static const char* findCi(const char* p, size_t n, PGM_P needle) {
    const size_t m = strlen_P(needle);
    for (size_t i = 0; i + m <= n; i++) {
      size_t k = 0;
      while (k < m && tolower(static_cast<unsigned char>(p[i + k])) == pgm_read_byte(needle + k))
        k++;
      if (k == m)
        return p + i;
    }
    return nullptr;
  }

  Check check() {
    const size_t avail = _currentClient.available();
    const size_t n = _currentClient.peekAvailable();
    const char* p = _currentClient.peekBuffer();
    if (!p || n == 0)
      return Check::Ready; // kein Einblick: Bibliothek entscheidet
    const char* lineEnd = findCi(p, n, PSTR("\r\n"));
    if (!lineEnd) {
      if (n >= kMaxRequestLine)
        return Check::UriTooLong;
      return avail > n ? Check::Ready : Check::Incomplete;
    }
    if (static_cast<size_t>(lineEnd - p) > kMaxRequestLine)
      return Check::UriTooLong;
    const char* headEnd = findCi(p, n, PSTR("\r\n\r\n"));
    if (!headEnd) {
      // Kopf länger als der erste Empfangspuffer: selten, die Bibliothek
      // übernimmt, sobald weitere Daten da sind
      return avail > n ? Check::Ready : Check::Incomplete;
    }
    const size_t headLen = static_cast<size_t>(headEnd - p) + 4;
    const char* cl = findCi(p, headLen, PSTR("\r\ncontent-length:"));
    if (!cl)
      return Check::Ready;
    const char* v = cl + 17;
    while (*v == ' ')
      v++;
    if (*v < '0' || *v > '9')
      return Check::Bad;
    const long len = strtol(v, nullptr, 10);
    if (findCi(p, headLen, PSTR("multipart/form-data")))
      return Check::Ready; // Upload: blockierend
    if (len > kMaxPlainBody)
      return Check::TooLarge;
    return avail >= headLen + static_cast<size_t>(len) ? Check::Ready : Check::Incomplete;
  }

  void drop(PGM_P status) {
    if (status) {
      char line[96];
      const int n =
          snprintf_P(line, sizeof(line),
                     PSTR("HTTP/1.1 %S\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"), status);
      _currentClient.write(reinterpret_cast<const uint8_t*>(line), static_cast<size_t>(n));
    }
    _currentClient.stop();
    _currentClient = WiFiClient();
    _currentStatus = HC_NONE;
  }

  void trimStrings() {
    // Verschieben gibt den Puffer frei; Zuweisen eines leeren Strings nicht
    if (_currentUri.length() > kKeepStringMax)
      _currentUri = String();
    if (_hostHeader.length() > kKeepStringMax)
      _hostHeader = String();
    for (int i = 0; i < _headerKeysCount; i++)
      if (_currentHeaders[i].value.length() > kKeepStringMax)
        _currentHeaders[i].value = String();
  }
};
using ServerImpl = GuardedServer;
#endif

ServerImpl* g_server = nullptr;

class Req : public hal::Request {
public:
  explicit Req(ServerImpl& s) : s_(s) {
    const String u = s.uri();
    strncpy(path_, u.c_str(), sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';
    if (s.hasArg("plain"))
      body_ = s.arg("plain");
  }
  hal::HttpMethod method() const override {
    switch (s_.method()) {
    case HTTP_GET:
      return hal::HttpMethod::Get;
    case HTTP_POST:
      return hal::HttpMethod::Post;
    case HTTP_PUT:
      return hal::HttpMethod::Put;
    case HTTP_PATCH:
      return hal::HttpMethod::Patch;
    case HTTP_DELETE:
      return hal::HttpMethod::Delete;
    case HTTP_OPTIONS:
      return hal::HttpMethod::Options;
    default:
      return hal::HttpMethod::Other;
    }
  }
  const char* path() const override { return path_; }
  bool arg(const char* name, char* out, size_t cap) const override {
    if (!s_.hasArg(name))
      return false;
    const String v = s_.arg(name);
    strncpy(out, v.c_str(), cap - 1);
    out[cap - 1] = '\0';
    return true;
  }
  bool hasArg(const char* name) const override { return s_.hasArg(name); }
  const char* body() const override { return body_.length() ? body_.c_str() : nullptr; }
  size_t bodyLength() const override { return body_.length(); }
  bool header(const char* name, char* out, size_t cap) const override {
    if (!s_.hasHeader(name))
      return false;
    const String v = s_.header(name);
    if (v.length() == 0)
      return false;
    strncpy(out, v.c_str(), cap - 1);
    out[cap - 1] = '\0';
    return true;
  }

private:
  ServerImpl& s_;
  char path_[64];
  String body_; // Kopie: ESP32 liefert arg() als Wert
};

/// Gepufferte Antwort (512 B), chunked wenn die Länge unbekannt ist
class Res : public hal::Response {
public:
  explicit Res(ServerImpl& s) : s_(s) {}
  ~Res() {
    if (started_ && !ended_)
      end();
    if (!started_) {
      // Handler hat nichts gesendet
      s_.send(500, "text/plain", "keine Antwort");
    }
  }
  void header(const char* name, const char* value) override { s_.sendHeader(name, value); }
  void begin(int code, const char* type, int32_t len) override {
    if (started_)
      return;
    started_ = true;
    chunked_ = len < 0;
    s_.setContentLength(chunked_ ? CONTENT_LENGTH_UNKNOWN : static_cast<size_t>(len));
    s_.send(code, type, "");
  }
  void write(const char* d, size_t n) override {
    if (!started_)
      begin(200, "text/plain", -1);
    while (n) {
      const size_t take = n < sizeof(buf_) - len_ ? n : sizeof(buf_) - len_;
      memcpy(buf_ + len_, d, take);
      len_ += take;
      d += take;
      n -= take;
      if (len_ == sizeof(buf_))
        flush();
    }
  }
  using Sink::write;
  void writeFlash(const uint8_t* data, size_t len) override {
    flush();
    // In Stücken aus dem Flash in den Puffer
    while (len) {
      const size_t take = len < sizeof(buf_) ? len : sizeof(buf_);
      memcpy_P(buf_, data, take);
      s_.sendContent(buf_, take);
      data += take;
      len -= take;
      yield();
    }
  }
  void end() override {
    if (ended_)
      return;
    ended_ = true;
    flush();
    if (chunked_)
      s_.sendContent("");
  }
  bool started() const override { return started_; }

private:
  void flush() {
    if (len_)
      s_.sendContent(buf_, len_);
    len_ = 0;
  }
  ServerImpl& s_;
  static char buf_[512]; // eine Anfrage zur Zeit: statisch statt auf dem Stack
  size_t len_ = 0;
  bool started_ = false;
  bool ended_ = false;
  bool chunked_ = false;
};

char Res::buf_[512];

} // namespace

void WebServer::serve() {
  Req req(*g_server);
  Res res(*g_server);
  if (dispatch_)
    dispatch_(req, res, ctx_);
}

void WebServer::upload(hal::UploadHandler* h) {
  HTTPUpload& up = g_server->upload();
  switch (up.status) {
  case UPLOAD_FILE_START: {
    Req req(*g_server);
    uploadAccepted_ = h->uploadStart(req, up.name.c_str(), up.filename.c_str());
    break;
  }
  case UPLOAD_FILE_WRITE:
    if (uploadAccepted_)
      h->uploadData(up.buf, up.currentSize);
    break;
  case UPLOAD_FILE_END:
    if (uploadAccepted_)
      h->uploadEnd(up.totalSize);
    uploadAccepted_ = false;
    break;
  default:
    if (uploadAccepted_)
      h->uploadAborted();
    uploadAccepted_ = false;
    break;
  }
}

void WebServer::addUpload(const char* path, hal::UploadHandler* handler) {
  if (uploadCount_ < 2)
    uploads_[uploadCount_++] = {path, handler};
}

void WebServer::begin(uint16_t port, hal::RequestFn dispatch, void* ctx) {
  dispatch_ = dispatch;
  ctx_ = ctx;
  static ServerImpl server(port);
  g_server = &server;
#if defined(ARDUINO_ARCH_ESP8266)
  server.collectHeaders("Authorization", "If-None-Match", "Accept-Encoding", "Content-Type",
                        "Cookie");
#else
  static const char* headers[] = {"Authorization", "If-None-Match", "Accept-Encoding",
                                  "Content-Type", "Cookie"};
  server.collectHeaders(headers, 5);
#endif
  for (uint8_t i = 0; i < uploadCount_; i++) {
    hal::UploadHandler* h = uploads_[i].handler;
    server.on(uploads_[i].path, HTTP_POST, [this]() { serve(); }, [this, h]() { upload(h); });
  }
  server.onNotFound([this]() { serve(); });
  server.begin();
}

void WebServer::handle() {
  if (!g_server)
    return;
#if defined(ARDUINO_ARCH_ESP8266)
  g_server->loop();
#else
  g_server->handleClient();
#endif
}

void WebServer::stop() {
  if (g_server)
    g_server->stop();
}

} // namespace platform
} // namespace ps

#endif
