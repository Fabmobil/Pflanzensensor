/**
 * @file wifi_seite_admin.h
 * @brief Administrationsseite des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält Funktionsdeklarationen zur Generierung und Verarbeitung
 * der Administrationsseite des Pflanzensensors.
 */

#ifndef WIFI_SEITE_ADMIN_H
#define WIFI_SEITE_ADMIN_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

/**
 * @brief Sendet eine Einstellungsoption an den Webserver
 *
 * @param bezeichnung Die Bezeichnung der Einstellung
 * @param name Der Name des Eingabefeldes
 * @param wert Der aktuelle Wert der Einstellung
 */
void sendeEinstellung(const __FlashStringHelper* bezeichnung, const __FlashStringHelper* name, const String& wert);

/**
 * @brief Sendet eine Checkbox-Option an den Webserver
 *
 * @param bezeichnung Die Bezeichnung der Checkbox
 * @param name Der Name der Checkbox
 * @param status Der aktuelle Status der Checkbox (true/false)
 */
void sendeCheckbox(const __FlashStringHelper* bezeichnung, const __FlashStringHelper* name, const bool& status);

/**
 * @brief Sendet Schwellwert-Einstellungen an den Webserver
 *
 * @param prefix Das Präfix für die Eingabefeld-Namen
 * @param gruenUnten Unterer grüner Schwellwert
 * @param gruenOben Oberer grüner Schwellwert
 * @param gelbUnten Unterer gelber Schwellwert
 * @param gelbOben Oberer gelber Schwellwert
 */
void sendeSchwellwerte(const __FlashStringHelper* prefix, int gruenUnten, int gruenOben, int gelbUnten, int gelbOben);

/**
 * @brief Sendet die Einstellungen für einen Analogsensor an den Webserver
 *
 * @param titel Der Titel des Sensorabschnitts
 * @param prefix Das Präfix für die Eingabefeld-Namen
 * @param sensorName Der Name des Sensors
 * @param minimum Der Minimalwert des Sensors
 * @param maximum Der Maximalwert des Sensors
 * @param gruenUnten Unterer grüner Schwellwert
 * @param gruenOben Oberer grüner Schwellwert
 * @param gelbUnten Unterer gelber Schwellwert
 * @param gelbOben Oberer gelber Schwellwert
 * @param alarm Status des Alarms für diesen Sensor
 * @param messwert Aktueller Messwert des Sensors
 */
void sendeAnalogsensorEinstellungen(const __FlashStringHelper* titel, const __FlashStringHelper* prefix, const String& sensorName, int minimum, int maximum,
                                    int gruenUnten, int gruenOben, int gelbUnten, int gelbOben, bool alarm, int messwert);

/**
 * @brief Sendet Links an den Webserver
 */
void sendeLinks();

/**
 * @brief Generiert und sendet die Administrationsseite
 */
void WebseiteAdminAusgeben();

#endif // WIFI_SEITE_ADMIN_H
