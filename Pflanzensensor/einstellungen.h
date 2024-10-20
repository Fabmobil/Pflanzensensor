/**
 * @file einstellungen.h
 * @brief Konfigurationsdatei für den Fabmobil Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält alle Konfigurationseinstellungen für den Pflanzensensor,
 * einschließlich Modulaktivierungen, PIN-Definitionen und Variablendeklarationen.
 */

#ifndef EINSTELLUNGEN_H
#define EINSTELLUNGEN_H


#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Modulaktivierungen
#define MODUL_DISPLAY 1
#define MODUL_WIFI 1
#define MODUL_DHT 1
#define MODUL_BODENFEUCHTE 1
#define MODUL_LEDAMPEL 1
#define MODUL_HELLIGKEIT 1
#define MODUL_WEBHOOK 1
#define MODUL_ANALOG3 1
#define MODUL_ANALOG4 1
#define MODUL_ANALOG5 1
#define MODUL_ANALOG6 1
#define MODUL_ANALOG7 1
#define MODUL_ANALOG8 1

// Wenn Bodenfeuchte- und Lichtsensor verwendet werden, brauchen wir auch einen Analog-Multiplexer:
#if MODUL_BODENFEUCHTE && MODUL_HELLIGKEIT
  #define MODUL_MULTIPLEXER true
#else
  #define MODUL_MULTIPLEXER false //sonst nicht
#endif


// Timestamp

extern WiFiUDP ntpUDP;
extern NTPClient zeitClient;

// Logging-Einstellungen
extern char logLevel[8];
extern int logAnzahlEintraege;
extern int LogAnzahlWebseite;
extern bool logInDatei;

// Allgemeine Einstellungen
extern const long baudrateSeriell;
extern unsigned long intervallAnalog;

// Bodenfeuchte-Einstellungen
#if MODUL_BODENFEUCHTE
    extern char bodenfeuchteName[20];
    extern bool bodenfeuchteWebhook;
    extern int bodenfeuchteMinimum;
    extern int bodenfeuchteMaximum;
    extern int bodenfeuchteGruenUnten;
    extern int bodenfeuchteGruenOben;
    extern int bodenfeuchteGelbUnten;
    extern int bodenfeuchteGelbOben;
#endif

// Display-Einstellungen
#if MODUL_DISPLAY
    extern unsigned long intervallDisplay;
    extern const int displayBreite;
    extern const int displayHoehe;
    extern const int displayReset;
    extern const int displayAdresse;
    extern bool displayAn;
#endif

// DHT-Einstellungen
#if MODUL_DHT
    extern const int dhtPin;
    extern const int dhtSensortyp;
    extern unsigned long intervallDht;
    extern bool lufttemperaturWebhook;
    extern int lufttemperaturGruenUnten;
    extern int lufttemperaturGruenOben;
    extern int lufttemperaturGelbUnten;
    extern int lufttemperaturGelbOben;
    extern bool luftfeuchteWebhook;
    extern int luftfeuchteGruenUnten;
    extern int luftfeuchteGruenOben;
    extern int luftfeuchteGelbUnten;
    extern int luftfeuchteGelbOben;
#endif

// Helligkeits-Einstellungen
#if MODUL_HELLIGKEIT
    extern char helligkeitName[20];
    extern bool helligkeitWebhook;
    extern int helligkeitMinimum;
    extern int helligkeitMaximum;
    extern int helligkeitGruenUnten;
    extern int helligkeitGruenOben;
    extern int helligkeitGelbUnten;
    extern int helligkeitGelbOben;
#endif

// LED-Ampel-Einstellungen
#if MODUL_LEDAMPEL
    extern int ampelModus;
    extern bool ampelAn;
#endif

// Webhook-Einstellungen
#if MODUL_WEBHOOK
    extern bool webhookAn;
    extern int webhookFrequenz;
    extern int webhookPingFrequenz;
#endif

// WiFi-Einstellungen
#if MODUL_WIFI
    extern char wifiHostname[20];
    extern bool wifiAp;
    extern char wifiApSsid[40];
    extern bool wlanNeustartGeplant;
    extern unsigned long geplanteWLANNeustartZeit;
    extern int wifiVerbindungsVersuche;
    extern char aktuelleSsid[40];
#endif

// Analog-Sensor-Einstellungen
#if MODUL_ANALOG3
    extern char analog3Name[20];
    extern bool analog3Webhook;
    extern int analog3Minimum;
    extern int analog3Maximum;
    extern int analog3GruenUnten;
    extern int analog3GruenOben;
    extern int analog3GelbUnten;
    extern int analog3GelbOben;
#endif

#if MODUL_ANALOG3
    extern char analog3Name[20];
    extern bool analog3Webhook;
    extern int analog3Minimum;
    extern int analog3Maximum;
    extern int analog3GruenUnten;
    extern int analog3GruenOben;
    extern int analog3GelbUnten;
    extern int analog3GelbOben;
#endif

#if MODUL_ANALOG4
    extern char analog4Name[20];
    extern bool analog4Webhook;
    extern int analog4Minimum;
    extern int analog4Maximum;
    extern int analog4GruenUnten;
    extern int analog4GruenOben;
    extern int analog4GelbUnten;
    extern int analog4GelbOben;
#endif

#if MODUL_ANALOG5
    extern char analog5Name[20];
    extern bool analog5Webhook;
    extern int analog5Minimum;
    extern int analog5Maximum;
    extern int analog5GruenUnten;
    extern int analog5GruenOben;
    extern int analog5GelbUnten;
    extern int analog5GelbOben;
#endif

#if MODUL_ANALOG6
    extern char analog6Name[20];
    extern bool analog6Webhook;
    extern int analog6Minimum;
    extern int analog6Maximum;
    extern int analog6GruenUnten;
    extern int analog6GruenOben;
    extern int analog6GelbUnten;
    extern int analog6GelbOben;
#endif

#if MODUL_ANALOG7
    extern char analog7Name[20];
    extern bool analog7Webhook;
    extern int analog7Minimum;
    extern int analog7Maximum;
    extern int analog7GruenUnten;
    extern int analog7GruenOben;
    extern int analog7GelbUnten;
    extern int analog7GelbOben;
#endif

#if MODUL_ANALOG8
    extern char analog8Name[20];
    extern bool analog8Webhook;
    extern int analog8Minimum;
    extern int analog8Maximum;
    extern int analog8GruenUnten;
    extern int analog8GruenOben;
    extern int analog8GelbUnten;
    extern int analog8GelbOben;
#endif


// Globale Variablen
extern const char* pflanzensensorVersion;
extern int neustarts;
extern unsigned long millisAktuell;
extern unsigned long millisVorherAnalog;
extern unsigned long millisVorherDht;
extern unsigned long millisVorherLedampel;
extern unsigned long millisVorherDisplay;
extern unsigned long millisVorherWebhook;
extern unsigned long millisVorherWebhookPing;
extern int module;
extern char ip[23];
extern const uint32_t wifiTimeout;

extern int bodenfeuchteMesswert;
extern int bodenfeuchteMesswertProzent;
extern float lufttemperaturMesswert;
extern float luftfeuchteMesswert;
extern int helligkeitMesswert;
extern int helligkeitMesswertProzent;
extern int analog3Messwert;
extern int analog3MesswertProzent;
extern int analog4Messwert;
extern int analog4MesswertProzent;
extern int analog5Messwert;
extern int analog5MesswertProzent;
extern int analog6Messwert;
extern int analog6MesswertProzent;
extern int analog7Messwert;
extern int analog7MesswertProzent;
extern int analog8Messwert;
extern int analog8MesswertProzent;
extern char helligkeitFarbe[8];
extern char bodenfeuchteFarbe[8];
extern char luftfeuchteFarbe[8];
extern char lufttemperaturFarbe[8];
extern char analog3Farbe[8];
extern char analog4Farbe[8];
extern char analog5Farbe[8];
extern char analog6Farbe[8];
extern char analog7Farbe[8];
extern char analog8Farbe[8];

// Pin-Definitionen
#define pinAnalog A0

#if MODUL_LEDAMPEL
    extern const int ampelPinRot;
    extern const int ampelPinGelb;
    extern const int ampelPinGruen;
#endif

#if MODUL_MULTIPLEXER
    extern const int multiplexerPinA;
    extern const int multiplexerPinB;
    extern const int multiplexerPinC;
#endif

// Einbinden weiterer Header-Dateien
#include "logger.h"
#include "variablenspeicher.h"
#include "analogsensor.h"
#include "mutex.h"
extern mutex_t mutex;
#include "passwoerter.h"

void initialisiereZeit();
#endif // EINSTELLUNGEN_H
