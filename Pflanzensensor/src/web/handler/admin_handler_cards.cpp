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
  sendChunk(F("<div class='card'><h3>Debug-Einstellungen</h3>"));
  sendChunk(
      F("<form method='post' action='/admin/updateSettings' "
        "class='config-form'>"));
  sendChunk(F("<input type='hidden' name='section' value='debug'>"));
  // File logging
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='file_logging_enabled' "
        "name='file_logging_enabled' value='true'"));
  if (ConfigMgr.isFileLoggingEnabled()) sendChunk(F(" checked"));
  sendChunk(F("> Logs in Datei speichern</label></div>"));
  // Debug RAM
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='debug_ram' name='debug_ram' value='true'"));
  if (ConfigMgr.isDebugRAM()) sendChunk(F(" checked"));
  sendChunk(F("> Debug RAM</label></div>"));
  // Debug Measurement Cycle
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='debug_measurement_cycle' "
        "name='debug_measurement_cycle' value='true'"));
  if (ConfigMgr.isDebugMeasurementCycle()) sendChunk(F(" checked"));
  sendChunk(F("> Debug Measurement Cycle</label></div>"));
  // Debug Sensor
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='debug_sensor' name='debug_sensor' "
        "value='true'"));
  if (ConfigMgr.isDebugSensor()) sendChunk(F(" checked"));
  sendChunk(F("> Debug Sensor</label></div>"));
  // Debug Display
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='debug_display' name='debug_display' "
        "value='true'"));
  if (ConfigMgr.isDebugDisplay()) sendChunk(F(" checked"));
  sendChunk(F("> Debug Display</label></div>"));
  // Debug WebSocket
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='debug_websocket' name='debug_websocket' "
        "value='true'"));
  if (ConfigMgr.isDebugWebSocket()) sendChunk(F(" checked"));
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
    if (currentLevel == level) sendChunk(F(" selected"));
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
    sendChunk(
        F("<form action='/admin/downloadLog' method='GET' "
          "style='margin-top:8px;'>"));
    sendChunk(
        F("<button type='submit' class='button button-primary'>Log "
          "herunterladen</button>"));
    sendChunk(F("</form>"));
  }
  sendChunk(F("</div>"));
}

#if USE_MAIL
void AdminHandler::generateAndSendMailSettingsCard() {
  sendChunk(F("<div class='card'><h3>E-Mail-Einstellungen</h3>"));
  sendChunk(
      F("<form method='post' action='/admin/updateSettings' "
        "class='config-form'>"));
  sendChunk(F("<input type='hidden' name='section' value='mail'>"));

  // Mail enabled
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='mail_enabled' "
        "name='mail_enabled' value='true'"));
  if (ConfigMgr.isMailEnabled()) sendChunk(F(" checked"));
  sendChunk(F("> E-Mail-Funktionen aktivieren</label></div>"));

  // SMTP Host
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_host'>SMTP-Server:</label>"));
  sendChunk(
      F("<input type='text' id='smtp_host' name='smtp_host' value='"));
  sendChunk(ConfigMgr.getSmtpHost());
  sendChunk(F("' placeholder='smtp.gmail.com'>"));
  sendChunk(F("</div>"));

  // SMTP Port
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_port'>SMTP-Port:</label>"));
  sendChunk(
      F("<input type='number' id='smtp_port' name='smtp_port' value='"));
  sendChunk(String(ConfigMgr.getSmtpPort()));
  sendChunk(F("' placeholder='587' min='1' max='65535'>"));
  sendChunk(F("</div>"));

  // SMTP User
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_user'>Benutzername/E-Mail:</label>"));
  sendChunk(
      F("<input type='email' id='smtp_user' name='smtp_user' value='"));
  sendChunk(ConfigMgr.getSmtpUser());
  sendChunk(F("' placeholder='your.email@gmail.com'>"));
  sendChunk(F("</div>"));

  // SMTP Password
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_password'>Passwort/App-Passwort:</label>"));
  sendChunk(
      F("<input type='password' id='smtp_password' name='smtp_password' value='"));
  sendChunk(ConfigMgr.getSmtpPassword());
  sendChunk(F("' placeholder='App-Passwort'>"));
  sendChunk(F("</div>"));

  // SMTP Sender Name
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_sender_name'>Absender-Name:</label>"));
  sendChunk(
      F("<input type='text' id='smtp_sender_name' name='smtp_sender_name' value='"));
  sendChunk(ConfigMgr.getSmtpSenderName());
  sendChunk(F("' placeholder='Pflanzensensor'>"));
  sendChunk(F("</div>"));

  // SMTP Sender Email
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_sender_email'>Absender-E-Mail:</label>"));
  sendChunk(
      F("<input type='email' id='smtp_sender_email' name='smtp_sender_email' value='"));
  sendChunk(ConfigMgr.getSmtpSenderEmail());
  sendChunk(F("' placeholder='pflanzensensor@your-domain.com'>"));
  sendChunk(F("</div>"));

  // SMTP Recipient
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label for='smtp_recipient'>Standard-Empfänger:</label>"));
  sendChunk(
      F("<input type='email' id='smtp_recipient' name='smtp_recipient' value='"));
  sendChunk(ConfigMgr.getSmtpRecipient());
  sendChunk(F("' placeholder='recipient@email.com'>"));
  sendChunk(F("</div>"));

  // SMTP STARTTLS
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='smtp_enable_starttls' "
        "name='smtp_enable_starttls' value='true'"));
  if (ConfigMgr.isSmtpEnableStartTLS()) sendChunk(F(" checked"));
  sendChunk(F("> STARTTLS-Verschlüsselung aktivieren</label></div>"));

  // SMTP Debug
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='smtp_debug' "
        "name='smtp_debug' value='true'"));
  if (ConfigMgr.isSmtpDebug()) sendChunk(F(" checked"));
  sendChunk(F("> SMTP-Debug-Ausgabe aktivieren</label></div>"));

  // Send Test Mail on Boot
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(
      F("<input type='checkbox' id='smtp_send_test_mail_on_boot' "
        "name='smtp_send_test_mail_on_boot' value='true'"));
  if (ConfigMgr.isSmtpSendTestMailOnBoot()) sendChunk(F(" checked"));
  sendChunk(F("> Test-Mail beim Systemstart senden</label></div>"));

  // Save handled automatically via AJAX; keep form for fallback but remove visible submit button
  sendChunk(F("</form>"));

  // Add test mail button
  if (ConfigMgr.isMailEnabled()) {
    sendChunk(
        F("<form action='/admin/testMail' method='POST' "
          "style='margin-top:8px;'>"));
    sendChunk(
        F("<button type='submit' class='button button-secondary'>Test-Mail "
          "senden</button>"));
    sendChunk(F("</form>"));
  }

  sendChunk(F("</div>"));
}
#endif

void AdminHandler::generateAndSendSystemSettingsCard() {
  sendChunk(F("<div class='card'><h3>Systemeinstellungen</h3>"));
  sendChunk(
      F("<form method='post' action='/admin/updateSettings' "
        "class='config-form'>"));
  sendChunk(F("<input type='hidden' name='section' value='system'>"));
  // Device name field
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label>Gerätename:</label>"));
  sendChunk(F("<input type='text' name='device_name' maxlength='32' value='"));
  sendChunk(ConfigMgr.getDeviceName());
  sendChunk(F("' autocomplete='off'></div>"));
  // MD5 verification checkbox
  sendChunk(F("<div class='form-group'><label class='checkbox-label'>"));
  sendChunk(F("<input type='checkbox' name='md5_verification'"));
  if (ConfigMgr.isMD5Verification()) sendChunk(F(" checked"));
  sendChunk(F("> MD5-Überprüfung für Updates aktivieren</label></div>"));
  // Save handled automatically via AJAX; keep form for fallback but remove visible submit button
  sendChunk(F("</form></div>"));
}

void AdminHandler::generateAndSendSystemActionsCard() {
  sendChunk(F("<div class='card'><h3>Systemaktionen</h3>"));
  sendChunk(F("<div class='button-group'>"));
  sendChunk(F("<form action='/admin/reset' method='POST' class='inline'>"));
  sendChunk(
      F("<button type='submit' onclick='return confirm(\"Wirklich alle "
        "Einstellungen zurücksetzen?\")' class='button "
        "button-danger'>Einstellungen zurücksetzen</button></form>"));
  sendChunk(F("<form action='/admin/reboot' method='POST' class='inline'>"));
  sendChunk(
      F("<button type='submit' onclick='return confirm(\"Gerät wirklich neu "
        "starten?\")' class='button button-warning'>Neustart "
        "durchführen</button></form>"));
  if (ConfigMgr.isFileLoggingEnabled()) {
    sendChunk(
        F("<form action='/admin/downloadLog' method='GET' class='inline'>"));
    sendChunk(
        F("<button type='submit' class='button button-primary'>Log "
          "herunterladen</button>"));
    sendChunk(F("</form>"));
  }
  // Add download/upload for settings and sensors JSON
  sendChunk(F("<form action='/admin/downloadConfig' method='GET' class='inline'>"));
  sendChunk(F("<button type='submit' class='button'>Einstellungen herunterladen</button>"));
  sendChunk(F("</form>"));
  sendChunk(F("<form action='/admin/downloadSensors' method='GET' class='inline'>"));
  sendChunk(F("<button type='submit' class='button'>Sensordaten herunterladen</button>"));
  sendChunk(F("</form>"));

  // Upload forms
  sendChunk(F("<div class='measurement-card'>"));
  sendChunk(F("<form id='upload-config-form' action='/admin/uploadConfig' method='POST' enctype='multipart/form-data' class='inline' style='display:inline-block;margin-left:8px;'>"));
  sendChunk(F("<input type='file' name='file' accept='.json' required>"));
  sendChunk(F("<button type='submit' class='button button-secondary'>Einstellungen oder Sensordaten hochladen</button>"));
  sendChunk(F("</form>"));
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
  sendChunk(WiFi.SSID());
  sendChunk(F("</td></tr><tr><td>WiFi Signal</td><td>"));
  sendChunk(String(WiFi.RSSI()));
  sendChunk(F(" dBm</td></tr><tr><td>IP Adresse</td><td>"));
  sendChunk(WiFi.localIP().toString());
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

void AdminHandler::generateAndSendLedTrafficLightSettingsCard() {
#if USE_LED_TRAFFIC_LIGHT
  sendChunk(F("<div class='card'><h3>LED-Ampel Einstellungen</h3>"));
  sendChunk(
      F("<form method='post' action='/admin/updateSettings' "
        "class='config-form'>"));
  sendChunk(
      F("<input type='hidden' name='section' value='led_traffic_light'>"));

  // Mode selection
  sendChunk(F("<div class='form-group'>"));
  sendChunk(F("<label>LED-Ampel Modus:</label>"));
  sendChunk(F("<select name='led_traffic_light_mode'>"));
  sendChunk(F("<option value='0'"));
  if (ConfigMgr.getLedTrafficLightMode() == 0) sendChunk(F(" selected"));
  sendChunk(F(">Modus 0: LED-Ampel aus</option>"));
  sendChunk(F("<option value='1'"));
  if (ConfigMgr.getLedTrafficLightMode() == 1) sendChunk(F(" selected"));
  sendChunk(F(">Modus 1: Alle Messungen anzeigen</option>"));
  sendChunk(F("<option value='2'"));
  if (ConfigMgr.getLedTrafficLightMode() == 2) sendChunk(F(" selected"));
  sendChunk(F(">Modus 2: Nur ausgewählte Messung anzeigen</option>"));
  sendChunk(F("</select>"));
  sendChunk(F("</div>"));

  // Measurement selection (only visible in mode 2)
  sendChunk(F("<div class='form-group' id='measurement_selection_group'"));
  if (ConfigMgr.getLedTrafficLightMode() != 2) {
    sendChunk(F(" style='display: none;'"));
  }
  sendChunk(F(">"));
  sendChunk(
      F("<label for='led_traffic_light_measurement'>Ausgewählte "
        "Messung:</label>"));
  sendChunk(
      F("<select name='led_traffic_light_measurement' "
        "id='led_traffic_light_measurement'>"));
  sendChunk(F("<option value=''>-- Messung auswählen --</option>"));

  // Get available measurements from sensor manager
  extern std::unique_ptr<SensorManager> sensorManager;
  if (sensorManager) {
    for (const auto& sensor : sensorManager->getSensors()) {
      if (sensor && sensor->isEnabled()) {
        String sensorId = sensor->getId();
        String sensorName = sensor->getName();

        // Get all measurements for this sensor
        for (size_t i = 0; i < sensor->config().activeMeasurements; i++) {
          String measurementName = sensor->getMeasurementName(i);
          String fieldName = sensor->config().measurements[i].fieldName;

          // Create measurement identifier
          String measurementId = sensorId + "_" + String(i);

          // Create display name
          String displayName = sensorName;
          if (!measurementName.isEmpty()) {
            displayName += " - " + measurementName;
          }
          if (!fieldName.isEmpty()) {
            displayName += " (" + fieldName + ")";
          }

          sendChunk(F("<option value='"));
          sendChunk(measurementId);
          sendChunk(F("'"));
          if (ConfigMgr.getLedTrafficLightSelectedMeasurement() ==
              measurementId)
            sendChunk(F(" selected"));
          sendChunk(F(">"));
          sendChunk(displayName);
          sendChunk(F("</option>"));
        }
      }
    }
  }
  sendChunk(F("</select>"));
  sendChunk(F("</div>"));

  // Save handled automatically via AJAX; keep form for fallback but remove visible submit button
  sendChunk(F("</form>"));

  // Add JavaScript to show/hide measurement selection based on mode
  sendChunk(F("<script>"));
  sendChunk(F("document.addEventListener('DOMContentLoaded', function() {"));
  sendChunk(
      F("  const modeSelect = "
        "document.querySelector('select[name=\"led_traffic_light_mode\"]');"));
  sendChunk(
      F("  const measurementGroup = "
        "document.getElementById('measurement_selection_group');"));
  sendChunk(F("  function toggleMeasurementSelection() {"));
  sendChunk(
      F("    if (modeSelect.value === '2') {"));  // Changed to '2' for mode 2
  sendChunk(F("      measurementGroup.style.display = 'block';"));
  sendChunk(F("    } else {"));
  sendChunk(F("      measurementGroup.style.display = 'none';"));
  sendChunk(F("    }"));
  sendChunk(F("  }"));
  sendChunk(F(
      "  modeSelect.addEventListener('change', toggleMeasurementSelection);"));
  sendChunk(F("  toggleMeasurementSelection();"));
  sendChunk(F("});"));
  sendChunk(F("</script>"));

  sendChunk(F("</div>"));
#endif  // USE_LED_TRAFFIC_LIGHT
}
