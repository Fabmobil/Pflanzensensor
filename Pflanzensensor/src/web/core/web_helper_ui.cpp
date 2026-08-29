/**
 * @file web_helper_ui.cpp
 * @brief Implementierung der UI-Komponenten
 */

#include "web/core/web_helper_ui.h"

namespace UI {

String statusBadge(const String& sensorId, const String& status, bool isError) {
  String cssClass = isError ? F("badge badge-error") : F("badge badge-ok");
  return String(F("<span class=\"")) + cssClass + F("\" data-sensor=\"") + sensorId + F("\">") +
         status + String(F("</span>"));
}

String sensorCard(const String& sensorId, const String& sensorName, const String& status,
                  const String& value) {
  return String(F("<div class=\"sensor-card\" data-id=\"")) + sensorId + F("\">") +
         String(F("<h3>")) + sensorName + String(F("</h3>")) + F("<div class=\"sensor-value\">") +
         value + String(F("</div>")) + F("<div class=\"sensor-status\">") + status +
         String(F("</div>")) + F("</div>");
}

String measurementRow(size_t index, const String& name, const String& value, const String& unit) {
  return String(F("<tr><td>")) + String(index) + String(F("</td><td>")) + name +
         String(F("</td><td>")) + value + String(F("</td><td>")) + unit + String(F("</td></tr>"));
}

String thresholdInputs(const String& label, const String& fieldName, float minValue,
                       float maxValue) {
  String minStr = String(minValue, 2);
  String maxStr = String(maxValue, 2);
  return String(F("<div class=\"threshold-inputs\"><label>")) + label + String(F(" Min:</label>")) +
         F("<input type=\"number\" name=\"") + fieldName + F("_min\" value=\"") + minStr +
         F("\"><label>Max:</label>") + F("<input type=\"number\" name=\"") + fieldName +
         F("_max\" value=\"") + maxStr + F("\"></div>");
}

String actionButton(const String& label, const String& action, const String& cssClass,
                    const String& confirmMessage) {
  String onclick = confirmMessage.isEmpty()
                       ? ""
                       : String(F(" onclick=\"return confirm('")) + confirmMessage + F("')\"");
  return String(F("<a href=\"")) + action + F("\" class=\"") + cssClass + F("\"") + onclick +
         String(F(">")) + label + String(F("</a>"));
}

String backLink(const String& url, const String& label) {
  return String(F("<a href=\"")) + url + F("\" class=\"link-back\">&larr; ") + label +
         String(F("</a>"));
}

String messageBox(const String& message, bool isError) {
  String cssClass = isError ? F("message-box error") : F("message-box success");
  return String(F("<div class=\"")) + cssClass + F("\">") + message + String(F("</div>"));
}

String spinner() {
  return String(F("<div class=\"spinner\"><div class=\"spinner-inner\"></div></div>"));
}

String emptyState(const String& message) {
  return String(F("<div class=\"empty-state\"><p>")) + message + String(F("</p></div>"));
}

} // namespace UI