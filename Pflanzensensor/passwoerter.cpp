/**
 * @file passwoerter.cpp
 * @brief Passwort- und Sicherheitskonfiguration für den Pflanzensensor (Implementierungsdatei)
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält die Implementierung der sensiblen Informationen wie Passwörter und Zugangsdaten.
 * Sie wird nicht im öffentlichen Repository gespeichert.
 */

#include "passwoerter.h"

#if MODUL_WEBHOOK
String webhookDomain = "hook.eu2.make.com";
String webhookPfad = "/tfe8kh229kog89riw66aa1clm0wtfwx2"; // Telegram
//String webhookPfad = "/7a3mxtmkoxi4jllf6qxbbr26y3vbwuzq"; // Mail
#endif

//#if MODUL_WIFI
bool wifiApPasswortAktiviert = false; // Soll das selbst aufgemachte WLAN ein Passwort haben?
String wifiApPasswort = "geheim"; // Das Passwort für das selbst aufgemachte WLAN
String wifiAdminPasswort = "admin"; // Passwort für das Admininterface
// Es können mehrere WLANs angegeben werden, mit denen sich der ESP verbinden soll.
String wifiSsid1 = "Fabmobil"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
String wifiPasswort1 = "NurFuerDieCoolenKids!"; // WLAN Passwort für das fremde Wifi
String wifiSsid2 = "Tommy"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
String wifiPasswort2 = "freibier"; // WLAN Passwort für das fremde Wifi
String wifiSsid3 = "Magrathea"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
String wifiPasswort3 = "Gemeinschaftskueche"; // WLAN Passwort für das fremde Wifi
//#endif

#if MODUL_INFLUXDB
// InfluxDB v2 Authentifizierungstoken:
String influxToken = "O24__XgbcJyoctWgsEjot6lW2Eh_xX-Jrw54cJ5YLssz8EIYAEd62Xgj_ulSeBeH4w-4o5PpLGbWeE7dpM8tcg==";
String influxOrganisation = "<your org>";  // InfluxDB v2 Organisationsname
String influxBucket = "<your bucket>"; //InfluxDB v2 Bucketname
String influxDatenbank = "collectd"; // InfluxDB v1 Datenbankname
String influxBenutzer = "collectd";  // InfluxDB v1 Benutzername
String influxPasswort = "collectd"; // InfluxDB v1 Passwort
#endif
