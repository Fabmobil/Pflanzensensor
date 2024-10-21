/**
 * @file einstellungen.cpp
 * @brief Implementierung der Konfigurationseinstellungen für den Fabmobil Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Definitionen und Initialisierungen aller Konfigurationseinstellungen
 * für den Pflanzensensor, die in einstellungen.h deklariert wurden.
 */

#include "einstellungen.h"

// Logging-Einstellungen
String logLevel = "info";
bool logInDatei = true;

// Allgemeine Einstellungen
const long baudrateSeriell = 115200; // 115200
unsigned long intervallMessung = 10000; // Messintervall in ms

// Bodenfeuchte-Einstellungen
#if MODUL_BODENFEUCHTE
  String bodenfeuchteName = "Bodenfeuchte";
  bool bodenfeuchteWebhook = true;
  int bodenfeuchteMinimum = 900;
  int bodenfeuchteMaximum = 380;
  int bodenfeuchteGruenUnten = 40;
  int bodenfeuchteGruenOben = 60;
  int bodenfeuchteGelbUnten = 20;
  int bodenfeuchteGelbOben = 80;
#endif

// Display-Einstellungen
#if MODUL_DISPLAY
  unsigned long intervallDisplay = 4874;
  const int displayBreite = 128;
  const int displayHoehe = 64;
  const int displayReset = -1;
  const int displayAdresse = 0x3C;
#endif

// DHT-Einstellungen
#if MODUL_DHT
  #include <DHT.h>
  const int dhtPin = 0;
  const int dhtSensortyp = DHT11;
  bool lufttemperaturWebhook = false;
  int lufttemperaturGruenUnten = 19;
  int lufttemperaturGruenOben = 22;
  int lufttemperaturGelbUnten = 17;
  int lufttemperaturGelbOben = 24;
  bool luftfeuchteWebhook = false;
  int luftfeuchteGruenUnten = 40;
  int luftfeuchteGruenOben = 60;
  int luftfeuchteGelbUnten = 20;
  int luftfeuchteGelbOben = 80;
#endif

// Helligkeits-Einstellungen
#if MODUL_HELLIGKEIT
  String helligkeitName = "Helligkeit";
  bool helligkeitWebhook = false;
  int helligkeitMinimum = 8;
  int helligkeitMaximum = 1024;
  int helligkeitGruenUnten = 40;
  int helligkeitGruenOben = 60;
  int helligkeitGelbUnten = 20;
  int helligkeitGelbOben = 80;
#endif

// LED-Ampel-Einstellungen
#if MODUL_LEDAMPEL
  int ampelModus = 1;
  bool ampelAn = true;
  const int ampelPinRot = 13;
  const int ampelPinGelb = 12;
  const int ampelPinGruen = 14;
#endif

// Webhook-Einstellungen
#if MODUL_WEBHOOK
  bool webhookAn = false;
  int webhookFrequenz = 12;
  int webhookPingFrequenz = 24;
#endif

// WiFi-Einstellungen
#if MODUL_WIFI
  String wifiHostname = "pflanzensensor";
  bool wifiAp = false;
  String wifiApSsid = "Fabmobil Pflanzensensor";
#endif

#if MODUL_MULTIPLEXER
  const int multiplexerPinA = 15;
  const int multiplexerPinB = 2;
  const int multiplexerPinC = 16;
#endif

// Analog-Sensor-Einstellungen
#if MODUL_ANALOG3
  String analog3Name = "Analog 3";
  bool analog3Webhook = false;
  int analog3Minimum = 900;
  int analog3Maximum = 380;
  int analog3GruenUnten = 40;
  int analog3GruenOben = 60;
  int analog3GelbUnten = 20;
  int analog3GelbOben = 80;
#endif

#if MODUL_ANALOG4 // wenn ein vierter Analogsensor verwendet wird
  String analog4Name = "Analog 4"; // Name des Sensors
  bool analog4Webhook = false; // soll der Sensor für Alarme überwacht werden?
  int analog4Minimum = 900; // Minimalwert des Sensors
  int analog4Maximum = 380; // Maximalwert des Sensors
  int analog4GruenUnten = 40;
  int analog4GruenOben = 60;
  int analog4GelbUnten = 20;
  int analog4GelbOben = 80;
#endif
#if MODUL_ANALOG5 // wenn ein fünfter Analogsensor verwendet wird
  String analog5Name = "Analog 5"; // Name des Sensors
  bool analog5Webhook = false; // soll der Sensor für Alarme überwacht werden?
  int analog5Minimum = 900; // Minimalwert des Sensors
  int analog5Maximum = 380; // Maximalwert des Sensors
  int analog5GruenUnten = 40;
  int analog5GruenOben = 60;
  int analog5GelbUnten = 20;
  int analog5GelbOben = 80;
#endif
#if MODUL_ANALOG6 // wenn ein sechster Analogsensor verwendet wird
  String analog6Name = "Analog 5"; // Name des Sensors
  bool analog6Webhook = false; // soll der Sensor für Alarme überwacht werden?
  int analog6Minimum = 900; // Minimalwert des Sensors
  int analog6Maximum = 380; // Maximalwert des Sensors
  int analog6GruenUnten = 40;
  int analog6GruenOben = 60;
  int analog6GelbUnten = 20;
  int analog6GelbOben = 80;
#endif
#if MODUL_ANALOG7 // wenn ein siebter Analogsensor verwendet wird
  String analog7Name = "Analog 7"; // Name des Sensors
  bool analog7Webhook = false; // soll der Sensor für Alarme überwacht werden?
  int analog7Minimum = 900; // Minimalwert des Sensors
  int analog7Maximum = 380; // Maximalwert des Sensors
  int analog7GruenUnten = 40;
  int analog7GruenOben = 60;
  int analog7GelbUnten = 20;
  int analog7GelbOben = 80;
#endif
#if MODUL_ANALOG8 // wenn ein achter Analogsensor verwendet wird
  String analog8Name = "Analog 8"; // Name des Sensors
  bool analog8Webhook = false; // soll der Sensor für Alarme überwacht werden?
  int analog8Minimum = 900; // Minimalwert des Sensors
  int analog8Maximum = 380; // Maximalwert des Sensors
  int analog8GruenUnten = 40;
  int analog8GruenOben = 60;
  int analog8GelbUnten = 20;
  int analog8GelbOben = 80;
#endif

// Globale Variablen
const char* pflanzensensorVersion = "1.3.2";
int neustarts = 1;
unsigned long millisAktuell = 0;
unsigned long millisVorherAnalog = 0;
unsigned long millisVorherDht = 0;
unsigned long millisVorherLedampel = 0;
unsigned long millisVorherDisplay = 0;
unsigned long millisVorherWebhook = 0;
unsigned long millisVorherWebhookPing = 0;
int module = 0;
String ip = "keine WLAN Verbindung.";
const uint32_t wifiTimeout = 5000;
int analog3Messwert = -1;
int analog3MesswertProzent = -1;
int analog4Messwert = -1;
int analog4MesswertProzent = -1;
int analog5Messwert = -1;
int analog5MesswertProzent = -1;
int analog6Messwert = -1;
int analog6MesswertProzent = -1;
int analog7Messwert = -1;
int analog7MesswertProzent = -1;
int analog8Messwert = -1;
int analog8MesswertProzent = -1;

String helligkeitFarbe = "rot";
String bodenfeuchteFarbe = "rot";
String luftfeuchteFarbe = "rot";
String lufttemperaturFarbe = "rot";
String analog3Farbe = "rot";
String analog4Farbe = "rot";
String analog5Farbe = "rot";
String analog6Farbe = "rot";
String analog7Farbe = "rot";
String analog8Farbe = "rot";

#if MODUL_BODENFEUCHTE
  int bodenfeuchteMesswert = -1;
  int bodenfeuchteMesswertProzent = -1;
#else
  int bodenfeuchteMesswert = -1;
  int bodenfeuchteMesswertProzent = -1;
#endif

#if MODUL_DHT
  float lufttemperaturMesswert = -1;
  float luftfeuchteMesswert = -1;
#endif

#if MODUL_HELLIGKEIT
  int helligkeitMesswert = -1;
  int helligkeitMesswertProzent = -1;
#endif

#if MODUL_WIFI
  String aktuelleSsid = "";
  int wifiVerbindungsVersuche = 0;
  unsigned long geplanteWLANNeustartZeit = 0;
  bool wlanNeustartGeplant = false;
#endif

#if MODUL_DISPLAY
  int status = 0;
  bool displayAn = true;
#endif

mutex_t mutex;
