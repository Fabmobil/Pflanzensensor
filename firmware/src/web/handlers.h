/**
 * @file handlers.h
 * @brief Alle Handler der statischen Routentabelle
 * @details Zustandslos; sie schreiben direkt in die Antwort. `rest` ist bei
 *          Präfix-Routen der Pfadteil hinter dem Präfix, sonst "".
 */
#pragma once

#include "hal/http_server.h"
#include "web/context.h"

namespace ps {
namespace web {

using Handler = void (*)(WebContext& c, hal::Request& req, hal::Response& res, const char* rest);

// Öffentlich
void getLatestValues(WebContext&, hal::Request&, hal::Response&, const char*);
void metrics(WebContext&, hal::Request&, hal::Response&, const char*);
void status(WebContext&, hal::Request&, hal::Response&, const char*);
void system(WebContext&, hal::Request&, hal::Response&, const char*);
void measurePublic(WebContext&, hal::Request&, hal::Response&, const char*);
void chronikData(WebContext&, hal::Request&, hal::Response&, const char*);
void chronikInfo(WebContext&, hal::Request&, hal::Response&, const char*);
void chronikCsv(WebContext&, hal::Request&, hal::Response&, const char*);
// Admin: Konfiguration und System
void configGet(WebContext&, hal::Request&, hal::Response&, const char*);
void configPatch(WebContext&, hal::Request&, hal::Response&, const char*);
void configExport(WebContext&, hal::Request&, hal::Response&, const char*);
void configImport(WebContext&, hal::Request&, hal::Response&, const char*);
void secretAdmin(WebContext&, hal::Request&, hal::Response&, const char*);
void secretWifi(WebContext&, hal::Request&, hal::Response&, const char*);
void secretMailKey(WebContext&, hal::Request&, hal::Response&, const char*);
void reboot(WebContext&, hal::Request&, hal::Response&, const char*);
void factoryReset(WebContext&, hal::Request&, hal::Response&, const char*);
void logsGet(WebContext&, hal::Request&, hal::Response&, const char*);
void logsFile(WebContext&, hal::Request&, hal::Response&, const char*);
void logsSettings(WebContext&, hal::Request&, hal::Response&, const char*);
void login(WebContext&, hal::Request&, hal::Response&, const char*);
void logout(WebContext&, hal::Request&, hal::Response&, const char*);
// Admin: Sensoren
void sensorsGet(WebContext&, hal::Request&, hal::Response&, const char*);
void sensorsPatch(WebContext&, hal::Request&, hal::Response&, const char*);
void sensorsAction(WebContext&, hal::Request&, hal::Response&, const char*);
// Admin: Mail
void mailTest(WebContext&, hal::Request&, hal::Response&, const char*);
void mailStatus(WebContext&, hal::Request&, hal::Response&, const char*);
void mailTemplates(WebContext&, hal::Request&, hal::Response&, const char*);
// Admin: OTA (nach dem Upload)
void otaDone(WebContext&, hal::Request&, hal::Response&, const char*);
void otaStatus(WebContext&, hal::Request&, hal::Response&, const char*);

} // namespace web
} // namespace ps
