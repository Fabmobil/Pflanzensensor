/**
 * @file passwoerter.h
 * @brief Passwort- und Sicherheitskonfiguration für den Pflanzensensor (Header-Datei)
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält Deklarationen für sensible Informationen wie Passwörter und Zugangsdaten.
 * Sie wird nicht im öffentlichen Repository gespeichert.
 */

#ifndef PASSWOERTER_H
#define PASSWOERTER_H

#include <Arduino.h>


extern String webhookDomain;
extern String webhookPfad;



extern bool wifiApPasswortAktiviert;
extern String wifiApPasswort;
extern String wifiAdminPasswort;
extern String wifiSsid1;
extern String wifiPasswort1;
extern String wifiSsid2;
extern String wifiPasswort2;
extern String wifiSsid3;
extern String wifiPasswort3;


#if MODUL_INFLUXDB
extern String influxToken;
extern String influxOrganisation;
extern String influxBucket;
extern String influxDatenbank;
extern String influxBenutzer;
extern String influxPasswort;
#endif

#endif // PASSWOERTER_H
