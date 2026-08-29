/**
 * @file web_helper_params.h
 * @brief Parameter-Parser für AJAX-Requests
 * @details Zentrale Hilfsklasse für das Parsen von HTTP-Request-Parametern:
 *          - Type-safe Getter (String, int, float, bool)
 *          - Required-Validierung mit automatischem Error-Response
 *          - Default-Werte
 *          - Konsistente Fehlerbehandlung
 * 
 * Vorteile:
 * - DRY: Keine 81fachen Wiederholungen von _server.arg()
 * - Testbar: Logik isoliert von HTTP
 * - Konsistent: Einheitliche Fehlerbehandlung
 */

#ifndef WEB_HELPER_PARAMS_H
#define WEB_HELPER_PARAMS_H

#include "utils/platform_compat.h"
#include "web_helper_response.h"
#include <Arduino.h>

class ParamParser {
public:
  /**
   * @brief Konstruktor
   * @param server Referenz auf WebServer
   */
  explicit ParamParser(ESPWebServer& server) : m_server(server) {}

  // === Required Parameter (müssen vorhanden sein) ===

  /**
   * @brief Pflicht-String-Parameter abrufen
   * @param name Parametername
   * @return Wert oder leerer String wenn fehlend
   * @details Setzt m_error wenn Parameter fehlt
   */
  String getString(const String& name) {
    String val = m_server.arg(name);
    if (val.isEmpty() && m_server.arg(name).length() == 0) {
      // Parameter existiert aber ist leer
      m_error = true;
      m_errorField = name;
    }
    return val;
  }

  String requiredString(const String& name) {
    String val = m_server.arg(name);
    if (val.isEmpty()) {
      m_error = true;
      m_errorField = name;
    }
    return val;
  }

  /**
   * @brief Pflicht-Integer-Parameter
   */
  int requiredInt(const String& name, int defaultVal = 0) {
    String val = m_server.arg(name);
    if (val.isEmpty()) {
      m_error = true;
      m_errorField = name;
      return defaultVal;
    }
    return val.toInt();
  }

  /**
   * @brief Pflicht-Float-Parameter
   */
  float requiredFloat(const String& name, float defaultVal = 0.0f) {
    String val = m_server.arg(name);
    if (val.isEmpty()) {
      m_error = true;
      m_errorField = name;
      return defaultVal;
    }
    return val.toFloat();
  }

  // === Optionale Parameter mit Default-Werten ===

  /**
   * @brief Optionaler String mit Default
   */
  String optString(const String& name, const String& defaultVal = "") {
    String val = m_server.arg(name);
    return val.isEmpty() ? defaultVal : val;
  }

  /**
   * @brief Optionaler Integer mit Default
   */
  int optInt(const String& name, int defaultVal = 0) {
    String val = m_server.arg(name);
    return val.isEmpty() ? defaultVal : val.toInt();
  }

  /**
   * @brief Optionaler Float mit Default
   */
  float optFloat(const String& name, float defaultVal = 0.0f) {
    String val = m_server.arg(name);
    return val.isEmpty() ? defaultVal : val.toFloat();
  }

  /**
   * @brief Optionaler Boolean (true wenn "true", "1", "yes")
   */
  bool optBool(const String& name, bool defaultVal = false) {
    String val = m_server.arg(name);
    if (val.isEmpty())
      return defaultVal;
    return val == "true" || val == "1" || val == "yes";
  }

  // === Fehlerbehandlung ===

  /**
   * @brief Prüfen ob ein Fehler aufgetreten ist
   */
  bool hasError() const { return m_error; }

  /**
   * @brief Namen des fehlenden Parameters abrufen
   */
  String getErrorField() const { return m_errorField; }

  /**
   * @brief Fehler zurücksetzen
   */
  void clearError() {
    m_error = false;
    m_errorField = "";
  }

  /**
   * @brief Error-Response senden und zurückgeben
   */
  bool sendError(ESPWebServer& server) {
    if (!m_error)
      return false;
    server.send(400, "application/json", ResponseBuilder::invalidInput(m_errorField).c_str());
    return true;
  }

  /**
   * @brief Anzahl Parameter abrufen
   */
  int argCount() const { return m_server.args(); }

  /**
   * @brief Alle Argument-Namen auflisten (für Debug)
   */
  String getArgNames() const {
    String result;
    for (int i = 0; i < m_server.args(); i++) {
      if (i > 0)
        result += ", ";
      result += m_server.argName(i);
    }
    return result;
  }

private:
  ESPWebServer& m_server;
  bool m_error = false;
  String m_errorField;
};

#endif // WEB_HELPER_PARAMS_H