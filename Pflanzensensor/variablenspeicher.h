/**
 * @file variablenspeicher.h
 * @brief Funktionen zum Speichern und Laden von Variablen
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zum Speichern und Laden von Variablen
 * im Flash-Speicher des ESP8266.
 */

#ifndef VARIABLENSPEICHER_H
#define VARIABLENSPEICHER_H

#include <Arduino.h>
#include <Preferences.h>
extern Preferences variablen;
#include <FS.h>
#include <LittleFS.h>

// Funktionsdeklarationen
bool VariablenDa();
void VariablenSpeichern();
void VariablenLaden();
void VariablenLoeschen();
void VariablenAuflisten(File dir, int numTabs);

#endif // VARIABLENSPEICHER_H
