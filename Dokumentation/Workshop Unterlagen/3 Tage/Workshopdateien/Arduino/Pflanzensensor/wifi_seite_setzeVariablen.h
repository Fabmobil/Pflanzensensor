/**
 * @file wifi_seite_setzeVariablen.h
 * @brief Variablenverarbeitung für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält Funktionsdeklarationen zur Verarbeitung und Aktualisierung
 * von Variablen, die über die Weboberfläche geändert werden.
 */

#ifndef WIFI_SEITE_SETZE_VARIABLEN_H
#define WIFI_SEITE_SETZE_VARIABLEN_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer Webserver;
extern bool wlanAenderungVorgenommen;
extern String wifiAdminPasswort;

/**
 * @brief Gibt alle empfangenen POST-Argumente in der Konsole aus
 *
 * Diese Funktion ist nützlich für das Debugging von Formulareingaben.
 */
void ArgumenteAusgeben();

/**
 * @brief Verarbeitet die Änderungen, die auf der Administrationsseite vorgenommen wurden
 *
 * Diese Funktion überprüft das Passwort, aktualisiert die Variablen und
 * sendet eine Bestätigungsseite mit allen vorgenommenen Änderungen an den Client.
 * Es werden nur Änderungen angezeigt, bei denen tatsächlich etwas geändert wurde,
 * einschließlich Änderungen am WLAN-Modus und Checkboxen.
 */
void WebseiteSetzeVariablen();

/**
 * @brief Aktualisiert alle Variablen basierend auf den empfangenen POST-Daten
 */
void AktualisiereVariablen();

/**
 * @brief Aktualisiert die Einstellungen für einen spezifischen Analogsensor
 *
 * @param sensorNumber Die Nummer des zu aktualisierenden Analogsensors
 */
void AktualisiereAnalogsensor(int sensorNumber);

/**
 * @brief Aktualisiert einen String-Wert basierend auf den empfangenen POST-Daten
 *
 * @param argName Der Name des Arguments
 * @param wert Referenz auf die zu aktualisierende String-Variable
 * @param istWLANEinstellung Gibt an, ob es sich um eine WLAN-Einstellung handelt
 */
void AktualisiereString(const String& argName, String& wert, bool istWLANEinstellung = false);

/**
 * @brief Aktualisiert einen Integer-Wert basierend auf den empfangenen POST-Daten
 *
 * @param argName Der Name des Arguments
 * @param wert Referenz auf die zu aktualisierende Integer-Variable
 * @param istWLANEinstellung Gibt an, ob es sich um eine WLAN-Einstellung handelt
 */
void AktualisiereInteger(const String& argName, int& wert, bool istWLANEinstellung = false);

/**
 * @brief Aktualisiert einen Boolean-Wert basierend auf den empfangenen POST-Daten
 *
 * @param argName Der Name des Arguments
 * @param wert Referenz auf die zu aktualisierende Boolean-Variable
 * @param istWLANEinstellung Gibt an, ob es sich um eine WLAN-Einstellung handelt
 */
void AktualisiereBoolean(const String& argName, bool& wert, bool istWLANEinstellung = false);

#endif // WIFI_SEITE_SETZE_VARIABLEN_H
