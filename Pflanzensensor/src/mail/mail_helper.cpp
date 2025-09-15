/**
 * @file mail_helper.cpp
 * @brief Implementation der Mail Helper-Funktionen
 */

#include "mail_helper.h"

#if USE_MAIL

#include "mail_manager.h"
#include "../logger/logger.h"
#include "../managers/manager_config.h"
#include <ESP8266WiFi.h>

extern Logger logger; // Global logger instance

namespace MailHelper {

ResourceResult sendQuickTestMail() {
  logger.info(F("MailHelper"), F("Sende schnelle Test-E-Mail"));

  auto& mailManager = MailManager::getInstance();
  return mailManager.sendTestMail();
}

ResourceResult sendSystemInfo() {
  logger.info(F("MailHelper"), F("Sende System-Info E-Mail"));

  String subject = F("System Info - ") + ConfigMgr.getDeviceName();
  String message = getSystemInfoString();

  auto& mailManager = MailManager::getInstance();
  return mailManager.sendMail(subject, message);
}

bool isMailSystemReady() {
  // Prüfe WiFi-Verbindung
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  // Prüfe ob Manager initialisiert ist
  auto& mailManager = MailManager::getInstance();
  return mailManager.isHealthy();
}

String getSystemInfoString() {
  String info = F("=== SYSTEM INFORMATION ===\n\n");

  // Geräte-Info
  info += F("Gerätename: ") + ConfigMgr.getDeviceName() + F("\n");
  info += F("Firmware: ") + String(F(VERSION)) + F("\n");
  info += F("Build: ") + String(__DATE__) + F(" ") + String(__TIME__) + F("\n\n");

  // Netzwerk-Info
  info += F("=== NETZWERK ===\n");
  info += F("WiFi SSID: ") + WiFi.SSID() + F("\n");
  info += F("IP-Adresse: ") + WiFi.localIP().toString() + F("\n");
  info += F("MAC-Adresse: ") + WiFi.macAddress() + F("\n");
  info += F("Signal Stärke: ") + String(WiFi.RSSI()) + F(" dBm\n\n");

  // System-Info
  info += F("=== SYSTEM ===\n");
  info += F("Freier Heap: ") + String(ESP.getFreeHeap()) + F(" Bytes\n");
  info += F("Heap Fragmentierung: ") + String(ESP.getHeapFragmentation()) + F("%\n");
  info += F("Max freier Block: ") + String(ESP.getMaxFreeBlockSize()) + F(" Bytes\n");
  info += F("Chip ID: ") + String(ESP.getChipId(), HEX) + F("\n");
  info += F("CPU Frequenz: ") + String(ESP.getCpuFreqMHz()) + F(" MHz\n");
  info += F("Flash Größe: ") + String(ESP.getFlashChipSize()) + F(" Bytes\n");
  info += F("Sketch Größe: ") + String(ESP.getSketchSize()) + F(" Bytes\n");
  info += F("Freier Sketch Space: ") + String(ESP.getFreeSketchSpace()) + F(" Bytes\n");
  info += F("Uptime: ") + String(millis() / 1000) + F(" Sekunden\n\n");

  // Sensor-Info (falls aktiviert)
#if USE_DHT || USE_ANALOG
  info += F("=== SENSOREN ===\n");
#if USE_DHT
  info += F("DHT Sensor: Aktiviert\n");
#endif
#if USE_ANALOG
  info += F("Analog Sensoren: ") + String(ANALOG_SENSOR_COUNT) + F("\n");
#endif
#if USE_MULTIPLEXER
  info += F("Multiplexer: Aktiviert\n");
#endif
  info += F("\n");
#endif

  // Feature-Info
  info += F("=== FEATURES ===\n");
#if USE_DISPLAY
  info += F("Display: Aktiviert\n");
#endif
#if USE_LED_TRAFFIC_LIGHT
  info += F("LED Traffic Light: Aktiviert\n");
#endif
#if USE_WEBSERVER
  info += F("Webserver: Aktiviert\n");
#endif
#if USE_INFLUXDB
  info += F("InfluxDB: Aktiviert\n");
#endif
  info += F("E-Mail: Aktiviert\n\n");

  info += F("Mit freundlichen Grüßen,\n");
  info += F("Ihr Pflanzensensor\n");

  return info;
}

} // namespace MailHelper

#endif // USE_MAIL
