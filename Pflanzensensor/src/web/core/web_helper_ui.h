/**
 * @file web_helper_ui.h
 * @brief Zentrale HTML-Komponenten für wiederverwendbare UI-Elemente
 * @details Konsolidiert alle wiederkehrenden HTML-Strukturen:
 *          - Sensor-Karten
 *          - Messungs-Tabellen
 *          - Schwellwert-Eingaben
 *          - Admin-Buttons
 * 
 * Vorteile:
 * - DRY: Einmal definiert, überall verwendet
 * - Konsistent: Gleiche Darstellung überall
 * - Änderungen nur an einem Ort
 */

#ifndef WEB_HELPER_UI_H
#define WEB_HELPER_UI_H

#include <Arduino.h>

/**
 * @namespace UI
 * @brief Zentrale Sammlung wiederverwendbarer HTML-Komponenten
 */
namespace UI {

/**
   * @brief Sensor-Status-Badge
   * @param sensorId Sensor-ID
   * @param status Status-Text (z.B. "OK", "Error", "Disabled")
   * @param isError Ob es ein Fehlerzustand ist
   * @return HTML-String für Badge
   */
String statusBadge(const String& sensorId, const String& status, bool isError = false);

/**
   * @brief Einfache Sensor-Karte für Übersichtsseiten
   * @param sensorId Sensor-ID
   * @param sensorName Anzeigename
   * @param status Aktueller Status
   * @param value Letzter Messwert
   * @return HTML-String für Karte
   */
String sensorCard(const String& sensorId, const String& sensorName, const String& status,
                  const String& value);

/**
   * @brief Messungs-Zeile in einer Tabelle
   * @param index Messungs-Index
   * @param name Messungs-Name
   * @param value Aktueller Wert
   * @param unit Einheit
   * @return HTML-Table-Row
   */
String measurementRow(size_t index, const String& name, const String& value, const String& unit);

/**
   * @brief Schwellwert-Eingabefeld-Paar (min/max)
   * @param label Beschriftung
   * @param fieldName Feld-Name für Formular
   * @param minValue Aktueller Minimum-Wert
   * @param maxValue Aktueller Maximum-Wert
   * @return HTML-Formularfelder
   */
String thresholdInputs(const String& label, const String& fieldName, float minValue,
                       float maxValue);

/**
   * @brief Admin-Aktionsbutton
   * @param label Button-Beschriftung
   * @param action Action-URL
   * @param cssClass Zusätzliche CSS-Klassen
   * @param confirmMessage Bestätigungs-Message (optional)
   * @return HTML-Button
   */
String actionButton(const String& label, const String& action, const String& cssClass = "button",
                    const String& confirmMessage = "");

/**
   * @brief Zurück-Link
   * @param url Ziel-URL
   * @param label Link-Text
   * @return HTML-Link
   */
String backLink(const String& url, const String& label = "Zurück");

/**
   * @brief Erfolgs-/Fehlermeldungs-Box
   * @param message Nachricht
   * @param isError Ob Fehler (sonst Erfolg)
   * @return HTML-Div
   */
String messageBox(const String& message, bool isError = false);

/**
   * @brief Ladende Anzeige (Spinner)
   * @return HTML-Spinner
   */
String spinner();

/**
   * @brief Leere Zustand-Anzeige
   * @param message Nachricht
   * @return HTML-Hinweis
   */
String emptyState(const String& message);

} // namespace UI

#endif // WEB_HELPER_UI_H