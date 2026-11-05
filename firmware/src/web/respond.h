/**
 * @file respond.h
 * @brief Antwort-Helfer: JSON, Fehlerformat, Body-Parsing
 * @details Fehlerformat (refactor.md 5.9):
 *   {"ok":false,"error":"<code>","message":"<Klartext>","fields":{"<pfad>":"<code>"}}
 */
#pragma once

#include <ArduinoJson.h>

#include "core/config/schema.h"
#include "core/json_writer.h"
#include "hal/http_server.h"

namespace ps {
namespace web {

/// JSON-Antwort beginnen (chunked); danach JsonWriter auf res, dann res.end()
void beginJson(hal::Response& res, int code = 200);
/// {"ok":true}
void sendOk(hal::Response& res);
/// Fehlerantwort
void sendError(hal::Response& res, int code, Err err, const char* message = nullptr,
               const schema::FieldErrors* fields = nullptr);
/// HTTP-Status zu einem Fehler
int statusFor(Err e);
/// Rumpf als JSON-Objekt lesen; bei Fehler wurde schon geantwortet
bool parseBody(hal::Request& req, hal::Response& res, JsonDocument& doc);
/// Ganzzahliger Query-Parameter
bool argU32(hal::Request& req, const char* name, uint32_t& out);

} // namespace web
} // namespace ps
