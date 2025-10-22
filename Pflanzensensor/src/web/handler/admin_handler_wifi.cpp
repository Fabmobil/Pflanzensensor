/**
 * @file admin_handler_wifi.cpp
 * @brief WiFi configuration functionality for admin handler
 * @details Handles WiFi settings cards and WiFi configuration updates
 */

#include <LittleFS.h>

#include "configs/config.h"
#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/wifi.h" // For getActiveWiFiSlot()
#include "web/handler/admin_handler.h"

void AdminHandler::generateAndSendWiFiSettingsCard() {
  sendChunk(F("<div class='card'><h3>WiFi Einstellungen</h3>"));
  sendChunk(F("<form method='post' action='/admin/updateWiFi' class='config-form'>"));
  int activeSlot = getActiveWiFiSlot(); // 0-based, -1 if not connected
  for (int i = 1; i <= 3; ++i) {
    bool isActive = (activeSlot == (i - 1));
    // SSID field or notice
    sendChunk(F("<div class='form-group'>"));
    sendChunk(F("<label>SSID "));
    sendChunk(String(i));
    sendChunk(F(" :</label>"));
    if (isActive) {
      sendChunk(F("<div class='active-wifi-notice'>Aktive Verbindung – Bearbeitung "
                  "nicht möglich</div>"));
    } else {
      sendChunk(F("<input type='text' name='ssid"));
      sendChunk(String(i));
      sendChunk(F("' value='"));
      if (i == 1)
        sendChunk(ConfigMgr.getWiFiSSID1());
      else if (i == 2)
        sendChunk(ConfigMgr.getWiFiSSID2());
      else if (i == 3)
        sendChunk(ConfigMgr.getWiFiSSID3());
      sendChunk(F("' maxlength='32' autocomplete='off'>"));
    }
    sendChunk(F("</div>"));
    // Password field or notice
    sendChunk(F("<div class='form-group'>"));
    sendChunk(F("<label>Passwort "));
    sendChunk(String(i));
    sendChunk(F(" :</label>"));
    if (isActive) {
      sendChunk(F("<div class='active-wifi-notice'>Aktive Verbindung – Bearbeitung "
                  "nicht möglich</div>"));
    } else {
      sendChunk(F("<input type='password' name='pwd"));
      sendChunk(String(i));
      sendChunk(F("' value='"));
      if (i == 1)
        sendChunk(ConfigMgr.getWiFiPassword1());
      else if (i == 2)
        sendChunk(ConfigMgr.getWiFiPassword2());
      else if (i == 3)
        sendChunk(ConfigMgr.getWiFiPassword3());
      sendChunk(F("' maxlength='64' autocomplete='off'>"));
    }
    sendChunk(F("</div>"));
  }
  // AJAX-only: WiFi settings are updated via JavaScript fetch calls. The form
  // remains as a DOM container only and should not be submitted directly.
  sendChunk(F("</form></div>"));
}

void AdminHandler::handleWiFiUpdate() {
  bool changed = false;
  String changes;
  int activeSlot = getActiveWiFiSlot() + 1; // getActiveWiFiSlot is 0-based

  // Require AJAX-only updates
  if (!requireAjaxRequest())
    return;

  for (int i = 1; i <= 3; ++i) {
    if (i == activeSlot)
      continue; // Skip the active slot!
    String ssidArg = "ssid" + String(i);
    String pwdArg = "pwd" + String(i);
    bool hasSsid = _server.hasArg(ssidArg);
    bool hasPwd = _server.hasArg(pwdArg);
    String ssid = hasSsid ? _server.arg(ssidArg) : String();
    String pwd = hasPwd ? _server.arg(pwdArg) : String();
    if (i == 1) {
      if (hasSsid && ssid != ConfigMgr.getWiFiSSID1()) {
        auto r = ConfigMgr.setWiFiSSID1(ssid);
        if (!r.isSuccess()) {
          String payload = String("{\"success\":false,\"error\":\"") + r.getMessage() + "\"}";
          sendJsonResponse(400, payload);
          return;
        }
        changed = true;
        changes += F("<li>SSID 1 geändert</li>");
      }
      if (hasPwd && pwd != ConfigMgr.getWiFiPassword1()) {
        auto r = ConfigMgr.setWiFiPassword1(pwd);
        if (!r.isSuccess()) {
          String payload = String("{\"success\":false,\"error\":\"") + r.getMessage() + "\"}";
          sendJsonResponse(400, payload);
          return;
        }
        changed = true;
        changes += F("<li>Passwort 1 geändert</li>");
      }
    } else if (i == 2) {
      if (hasSsid && ssid != ConfigMgr.getWiFiSSID2()) {
        auto r = ConfigMgr.setWiFiSSID2(ssid);
        if (!r.isSuccess()) {
          String payload = String("{\"success\":false,\"error\":\"") + r.getMessage() + "\"}";
          sendJsonResponse(400, payload);
          return;
        }
        changed = true;
        changes += F("<li>SSID 2 geändert</li>");
      }
      if (hasPwd && pwd != ConfigMgr.getWiFiPassword2()) {
        auto r = ConfigMgr.setWiFiPassword2(pwd);
        if (!r.isSuccess()) {
          String payload = String("{\"success\":false,\"error\":\"") + r.getMessage() + "\"}";
          sendJsonResponse(400, payload);
          return;
        }
        changed = true;
        changes += F("<li>Passwort 2 geändert</li>");
      }
    } else if (i == 3) {
      if (hasSsid && ssid != ConfigMgr.getWiFiSSID3()) {
        auto r = ConfigMgr.setWiFiSSID3(ssid);
        if (!r.isSuccess()) {
          String payload = String("{\"success\":false,\"error\":\"") + r.getMessage() + "\"}";
          sendJsonResponse(400, payload);
          return;
        }
        changed = true;
        changes += F("<li>SSID 3 geändert</li>");
      }
      if (hasPwd && pwd != ConfigMgr.getWiFiPassword3()) {
        auto r = ConfigMgr.setWiFiPassword3(pwd);
        if (!r.isSuccess()) {
          String payload = String("{\"success\":false,\"error\":\"") + r.getMessage() + "\"}";
          sendJsonResponse(400, payload);
          return;
        }
        changed = true;
        changes += F("<li>Passwort 3 geändert</li>");
      }
    }
  }

  // Save changes if any were made
  if (changed) {
    auto result = ConfigMgr.saveConfig();
    if (!result.isSuccess()) {
      String payload = String("{\"success\":false,\"error\":\"") + result.getMessage() + "\"}";
      sendJsonResponse(500, payload);
      return;
    }
  }

  // Always respond with JSON
  if (changed) {
    String payload = String("{\"success\":true,\"changes\":\"") + changes + "\"}";
    sendJsonResponse(200, payload);
  } else {
    sendJsonResponse(200, F("{\"success\":true,\"message\":\"Keine Änderungen\"}"));
  }
}
