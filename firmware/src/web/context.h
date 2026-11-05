/**
 * @file context.h
 * @brief Was die Web-Handler brauchen (Referenzen aus dem Composition Root)
 */
#pragma once

#include "services/chronik_service.h"
#include "services/config_service.h"
#include "services/display_service.h"
#include "services/log_service.h"
#include "services/mail_service.h"
#include "services/metrics_service.h"
#include "services/net_service.h"
#include "services/ota_service.h"
#include "services/sensor_service.h"
#include "services/system_info.h"
#include "services/template_store.h"
#include "web/auth.h"

namespace ps {

struct WebContext {
  ConfigService& cfg;
  SensorService& sensors;
  StateStore& state;
  ChronikService& chronik;
  LogService& log;
  MailService& mail;
  TemplateStore& templates;
  NetService& net;
  SystemInfo& sys;
  MetricsService& metrics;
  OtaService& ota;
  Auth& auth;
  Events& events;
  hal::Fs& fs;
  bool otaRunning = false;
};

} // namespace ps
