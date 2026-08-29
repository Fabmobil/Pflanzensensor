/**
 * @file web_helper_response.h
 * @brief Zentrale Hilfsklasse für HTTP-Antworten
 * @details Konsolidiert alle JSON-Antwortmuster:
 *          - Erfolgreiche Antworten mit Daten
 *          - Fehlerantworten
 *          - Standard-Format für alle Handler
 * 
 * Vorteile:
 * - DRY: Keine doppelten JSON-Builds
 * - Konsistent: Einheitliches Antwortformat
 * - Testbar: Ohne HTTP-Server testbar
 */

#ifndef WEB_HELPER_RESPONSE_H
#define WEB_HELPER_RESPONSE_H

#include <Arduino.h>

/**
 * @class ResponseBuilder
 * @brief Zentrale Builder-Klasse für HTTP-Antworten
 * @details Statische Methoden für einheitliche JSON-Antworten.
 *          Alle Web-Handler sollten diese statt manueller JSON-Strings verwenden.
 */
class ResponseBuilder {
public:
  // === Erfolgreiche Antworten ===

  /**
   * @brief Einfache Erfolgs-Antwort ohne Daten
   * @return {"success":true}
   */
  static String ok();

  /**
   * @brief Erfolgs-Antwort mit Nachricht
   * @param message Beschreibende Nachricht
   * @return {"success":true,"message":"..."}
   */
  static String ok(const String& message);

  /**
   * @brief Erfolgs-Antwort mit Datenpaar
   * @param key Daten-Schlüssel
   * @param value Daten-Wert
   * @return {"success":true,"key":"value"}
   */
  static String ok(const String& key, const String& value);

  // === Fehler-Antworten ===

  /**
   * @brief Einfache Fehler-Antwort (generic)
   * @return {"success":false}
   */
  static String error();

  /**
   * @brief Fehler-Antwort mit Nachricht
   * @param message Fehlerbeschreibung
   * @return {"success":false,"error":"..."}
   */
  static String error(const String& message);

  /**
   * @brief Fehler-Antwort mit spezifischem Fehlercode
   * @param code HTTP-ähnlicher Fehlercode (z.B. 400, 404, 500)
   * @param message Fehlerbeschreibung
   * @return {"success":false,"error":"...","code":400}
   */
  static String error(int code, const String& message);

  // === Spezielle Antworten ===

  /**
   * @brief Antwort für nicht gefunden
   * @param resource Name des nicht gefundenen Objekts
   * @return {"success":false,"error":"... not found","code":404}
   */
  static String notFound(const String& resource);

  /**
   * @brief Antwort für ungültige Eingabe
   * @param field Feld das ungültig war
   * @return {"success":false,"error":"Invalid ...","code":400}
   */
  static String invalidInput(const String& field);

  /**
   * @brief Antwort für nicht autorisiert
   * @return {"success":false,"error":"Unauthorized","code":401}
   */
  static String unauthorized();

  /**
   * @brief Antwort für Rate-Limit überschritten
   * @return {"success":false,"error":"Rate limit exceeded","code":429}
   */
  static String rateLimited();

  // === Daten-Formatierung ===

  /**
   * @brief Einzelnes Datenpaar als JSON-String
   * @param key Schlüssel
   * @param value Wert
   * @return "\"key\":\"value\""
   */
  static String dataPair(const String& key, const String& value);

  /**
   * @briefnumber als JSON-String
   * @param key Schlüssel
   * @param value Zahl
   * @return "\"key\":123"
   */
  static String dataPair(const String& key, int value);

  /**
   * @brief Boolean als JSON-String
   * @param key Schlüssel
   * @param value true/false
   * @return "\"key\":true"
   */
  static String dataPair(const String& key, bool value);

private:
  ResponseBuilder() = default; // Nur statische Methoden
};

#endif // WEB_HELPER_RESPONSE_H