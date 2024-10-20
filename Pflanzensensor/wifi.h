/**
 * @file wifi.h
 * @brief WiFi-Modul und Webserver für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur WiFi-Verbindung und
 * zur Steuerung des integrierten Webservers.
 */

#ifndef WIFI_H
#define WIFI_H

#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "einstellungen.h"
#include "logger.h"


extern ESP8266WiFiMulti wifiMulti;
extern ESP8266WebServer Webserver;
extern bool wifiAp;
extern char wifiSsid1[35];
extern char wifiPasswort1[35];
extern char wifiSsid2[35];
extern char wifiPasswort2[35];
extern char wifiSsid3[35];
extern char wifiPasswort3[35];

// Funktionsdeklarationen
String WifiSetup(String hostname);
void WebseiteBild(const char* pfad, const char* mimeType);
void WebseiteCss();
void NeustartWLANVerbindung();
void VerzoegerterWLANNeustart();
void SetzeLogLevel();
void DownloadLog();
void LeseMesswerte();

#endif // WIFI_H
