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

#if MODUL_WEBHOOK
extern char webhookDomain[20];
extern char webhookPfad[35];
#endif

#if MODUL_WIFI
extern bool wifiApPasswortAktiviert;
extern char wifiApPasswort[35];
extern char wifiAdminPasswort[12];
extern char wifiSsid1[35];
extern char wifiPasswort1[35];
extern char wifiSsid2[35];
extern char wifiPasswort2[35];
extern char wifiSsid3[35];
extern char wifiPasswort3[35];
#endif

#if MODUL_INFLUXDB
extern char influxToken[100];
extern char influxOrganisation[35];
extern char influxBucket[35];
extern char influxDatenbank[35];
extern char influxBenutzer[35];
extern char influxPasswort[35];
#endif

#endif // PASSWOERTER_H
