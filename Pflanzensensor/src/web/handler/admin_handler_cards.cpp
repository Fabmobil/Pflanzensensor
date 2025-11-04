/**
 * @file admin_handler_cards.cpp
 * @brief HTML card generation for admin interface
 * @details Generates various admin interface cards including debug settings,
 *          system settings, system actions, system info, and JSON debug cards
 */

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "managers/manager_sensor.h"
#include "web/handler/admin_handler.h"

void AdminHandler::generateAndSendDebugSettingsCard() {
  sendChunk(F("<div class='card'><h3>Log Einstellungen</h3>"));
  sendChunk(F("<form method='post' action='/admin/updateSettings' "
              "class='config-form'>"));
  sendChunk(F("<input type='hidden' name='section' value='debug'>"));
  // File logging
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' id='file_logging_enabled' "
              "name='file_logging_enabled' value='true'"));
  if (ConfigMgr.isFileLoggingEnabled())
    sendChunk(F(" checked"));
  sendChunk(F("> Logs in Datei speichern</label></div>"));
  // Debug RAM
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' id='debug_ram' name='debug_ram' value='true'"));
  if (ConfigMgr.isDebugRAM())
    sendChunk(F(" checked"));
  sendChunk(F("> Debug RAM</label></div>"));
  // Debug Measurement Cycle
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' id='debug_measurement_cycle' "
              "name='debug_measurement_cycle' value='true'"));
  if (ConfigMgr.isDebugMeasurementCycle())
    sendChunk(F(" checked"));
  sendChunk(F("> Debug Messzyklus</label></div>"));
  // Debug Sensor
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' id='debug_sensor' name='debug_sensor' "
              "value='true'"));
  if (ConfigMgr.isDebugSensor())
    sendChunk(F(" checked"));
  sendChunk(F("> Debug Sensor</label></div>"));
  // Debug Display
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' id='debug_display' name='debug_display' "
              "value='true'"));
  if (ConfigMgr.isDebugDisplay())
    sendChunk(F(" checked"));
  sendChunk(F("> Debug Display</label></div>"));
  // Debug WebSocket
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' id='debug_websocket' name='debug_websocket' "
              "value='true'"));
  if (ConfigMgr.isDebugWebSocket())
    sendChunk(F(" checked"));
  sendChunk(F("> Debug WebSocket</label></div>"));
  // Log Level Selection
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='log_level'>Log Level:</label>"));
  sendChunk(F("<select id='log_level' name='log_level'>"));
  String currentLevel = ConfigMgr.getLogLevel();
  const char* levels[] = {"ERROR", "WARNING", "INFO", "DEBUG"};
  for (const char* level : levels) {
    sendChunk(F("<option value='"));
    sendChunk(level);
    sendChunk(F("'"));
    if (currentLevel == level)
      sendChunk(F(" selected"));
    sendChunk(F(">"));
    sendChunk(level);
    sendChunk(F("</option>"));
  }
  sendChunk(F("</select>"));
  sendChunk(F("</div>"));
  // Save handled automatically via AJAX; keep form for fallback but remove visible submit button
  sendChunk(F("</form>"));
  // Add Download Log button if file logging is enabled
  if (ConfigMgr.isFileLoggingEnabled()) {
    sendChunk(F("<form action='/admin/downloadLog' method='GET' "
                "style='margin-top:8px;'>"));
    sendChunk(F("<button type='submit' class='button button-primary'>Log "
                "herunterladen</button>"));
    sendChunk(F("</form>"));
  }
  sendChunk(F("</div>"));
}

void AdminHandler::generateAndSendSystemSettingsCard() {
  sendChunk(F("<div class='card'><h3>Systemeinstellungen</h3>"));
  sendChunk(F("<form method='post' action='/admin/updateSettings' "
              "class='config-form'>"));
  sendChunk(F("<input type='hidden' name='section' value='system'>"));
  // Device name field
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label>Gerätename:</label>"));
  sendChunk(F("<input type='text' name='device_name' maxlength='32' value='"));
  sendChunk(ConfigMgr.getDeviceName());
  sendChunk(F("' autocomplete='off'></div>"));
  // MD5 verification checkbox
  // sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  // sendChunk(F("<input type='checkbox' name='md5_verification'"));
  // if (ConfigMgr.isMD5Verification())
  //   sendChunk(F(" checked"));
  // sendChunk(F("> MD5-Überprüfung für Updates aktivieren</label></div>"));
  // Admin password change (entered twice, saved via explicit button)
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label>Administrator Passwort:</label>"));
  // Note: omit the 'name' attribute to avoid auto-save during typing. JS will
  // submit the admin_password parameter only when the user clicks Speichern.
  sendChunk(F("<input type='password' id='admin_password' placeholder='Neues Admin-Passwort' "
              "autocomplete='new-password' value=''>"));
  sendChunk(F("</div>"));
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label>Passwort wiederholen:</label>"));
  sendChunk(F("<input type='password' id='admin_password_confirm' placeholder='Passwort "
              "wiederholen' autocomplete='new-password' value=''>"));
  sendChunk(F("</div>"));
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<button type='button' id='save_admin_password' class='button "
              "button-primary'>Passwort speichern</button>"));
  sendChunk(F("</div>"));
  // Save handled automatically via AJAX; keep form for fallback but remove visible submit button
  sendChunk(F("</form></div>"));
}

void AdminHandler::generateAndSendSystemActionsCard() {
  sendChunk(F("<div class='card'><h3>Systemaktionen</h3>"));
  sendChunk(F("<div class='button-group'>"));
  sendChunk(F("<form action='/admin/reset' method='POST' class='inline'>"));
  sendChunk(F("<button type='submit' onclick='return confirm(\"Wirklich alle "
              "Einstellungen zurücksetzen?\")' class='button "
              "button-danger'>Einstellungen zurücksetzen</button></form>"));
  sendChunk(F("<form action='/admin/reboot' method='POST' class='inline'>"));
  sendChunk(F("<button type='submit' onclick='return confirm(\"Gerät wirklich neu "
              "starten?\")' class='button button-warning'>Neustart "
              "durchführen</button></form>"));
  if (ConfigMgr.isFileLoggingEnabled()) {
    sendChunk(F("<form action='/admin/downloadLog' method='GET' class='inline'>"));
    sendChunk(F("<button type='submit' class='button button-primary'>Log "
                "herunterladen</button>"));
    sendChunk(F("</form>"));
  }
  // NOTE: Download/upload buttons removed - configuration now in Preferences (EEPROM)
  // Users can edit configuration through the web interface at /admin and /admin/sensors
  sendChunk(F("</div></div></div>"));
}

void AdminHandler::generateAndSendSystemInfoCard() {
  sendChunk(F("<div class='card'><h3>System Information</h3>"));
  sendChunk(F("<table class='info-table'>"));
  // Memory
  sendChunk(F("<tr><td>Freier Heap</td><td>"));
  sendChunk(formatMemorySize(ESP.getFreeHeap()));
  sendChunk(F("</td></tr><tr><td>Heap Fragmentierung</td><td>"));
  sendChunk(String(ESP.getHeapFragmentation()));
  sendChunk(F("%</td></tr><tr><td>Max. Block-Größe</td><td>"));
  sendChunk(formatMemorySize(ESP.getMaxFreeBlockSize()));
  sendChunk(F("</td></tr>"));
  yield();
  sendChunk(F("<tr><td>Laufzeit</td><td>"));
  sendChunk(formatUptime());
  sendChunk(F("</td></tr>"));
  yield();
  sendChunk(F("<tr><td>WiFi SSID</td><td>"));
  sendChunk(Component::getDisplaySSID());
  sendChunk(F("</td></tr><tr><td>WiFi Signal</td><td>"));
  sendChunk(String(WiFi.RSSI()));
  sendChunk(F(" dBm</td></tr><tr><td>IP Adresse</td><td>"));
  // Use the display IP helper so AP-mode shows softAP IP
  sendChunk(Component::getDisplayIP());
  sendChunk(F("</td></tr><tr><td>MAC Adresse</td><td>"));
  sendChunk(WiFi.macAddress());
  sendChunk(F("</td></tr>"));
  yield();
  {
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
      sendChunk(F("<tr><td>Dateisystem Gesamt</td><td>"));
      sendChunk(formatMemorySize(fs_info.totalBytes));
      sendChunk(F("</td></tr><tr><td>Dateisystem Belegt</td><td>"));
      sendChunk(formatMemorySize(fs_info.usedBytes));
      sendChunk(F("</td></tr><tr><td>Dateisystem Frei</td><td>"));
      sendChunk(formatMemorySize(fs_info.totalBytes - fs_info.usedBytes));
      sendChunk(F("</td></tr>"));
      if (ConfigMgr.isFileLoggingEnabled() && LittleFS.exists("/log.txt")) {
        File logFile = LittleFS.open("/log.txt", "r");
        if (logFile) {
          size_t logSize = logFile.size();
          logFile.close();
          sendChunk(F("<tr><td>Log Datei Größe</td><td>"));
          sendChunk(formatMemorySize(logSize));
          sendChunk(F(" ("));
          if (MAX_LOG_FILE_SIZE > 0) {
            sendChunk(String((logSize * 100) / MAX_LOG_FILE_SIZE));
          } else {
            sendChunk(F("0"));
          }
          sendChunk(F("% belegt)</td></tr>"));
        }
      }
    } else {
      sendChunk(F("<tr><td>Dateisystem</td><td>Fehler beim Zugriff</td></tr>"));
    }
  }
  yield();
  sendChunk(F("</table></div>"));
}
