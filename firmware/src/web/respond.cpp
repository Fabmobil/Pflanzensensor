/**
 * @file respond.cpp
 */
#include "web/respond.h"

#include <stdlib.h>

namespace ps {
namespace web {

void beginJson(hal::Response& res, int code) {
  res.header("Cache-Control", "no-store");
  res.begin(code, "application/json", -1);
}

void sendOk(hal::Response& res) {
  beginJson(res);
  res.write("{\"ok\":true}");
  res.end();
}

int statusFor(Err e) {
  switch (e) {
  case Err::None:
    return 200;
  case Err::NotFound:
    return 404;
  case Err::Busy:
  case Err::NotReady:
  case Err::Disabled:
    return 409;
  case Err::Auth:
    return 401;
  case Err::Throttled:
    return 429;
  case Err::NoSpace:
    return 507;
  case Err::Io:
  case Err::Hardware:
  case Err::Unknown:
    return 500;
  case Err::Unsupported:
    return 501;
  default:
    return 400;
  }
}

void sendError(hal::Response& res, int code, Err err, const char* message,
               const schema::FieldErrors* fields) {
  beginJson(res, code);
  JsonWriter w(res);
  w.beginObject();
  w.kv(FS_("ok"), false);
  w.key(FS_("error")).value(errCode(err));
  w.key(FS_("message"));
  if (message)
    w.value(message);
  else
    w.value(errText(err));
  if (fields && !fields->empty()) {
    w.key(FS_("fields"));
    fields->writeJson(w);
  }
  w.endObject();
  res.end();
}

bool parseBody(hal::Request& req, hal::Response& res, JsonDocument& doc) {
  const char* body = req.body();
  if (!body || !req.bodyLength()) {
    sendError(res, 400, Err::InvalidArgument, "Leerer Rumpf");
    return false;
  }
  if (req.bodyLength() > 6144) {
    sendError(res, 413, Err::TooLong, "Rumpf zu groß");
    return false;
  }
  const DeserializationError e = deserializeJson(doc, body, req.bodyLength());
  if (e != DeserializationError::Ok || !doc.is<JsonObject>()) {
    sendError(res, 400, Err::ParseError, "Kein gültiges JSON-Objekt");
    return false;
  }
  return true;
}

bool argU32(hal::Request& req, const char* name, uint32_t& out) {
  char buf[16];
  if (!req.arg(name, buf, sizeof(buf)))
    return false;
  char* end = nullptr;
  const unsigned long v = strtoul(buf, &end, 10);
  if (!end || *end)
    return false;
  out = static_cast<uint32_t>(v);
  return true;
}

} // namespace web
} // namespace ps
