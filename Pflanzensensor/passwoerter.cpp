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

char webhookDomain[20] = "hook.eu2.make.com";
char webhookPfad[35] = "/tfe8kh229kog89riw66aa1clm0wtfwx2"; // Telegram
//char webhookPfad[35] = "/7a3mxtmkoxi4jllf6qxbbr26y3vbwuzq"; // Mail


bool wifiApPasswortAktiviert = false; // Soll das selbst aufgemachte WLAN ein Passwort haben?
char wifiApPasswort[35] = "geheim"; // Das Passwort für das selbst aufgemachte WLAN
char wifiAdminPasswort[12] = "admin"; // Passwort für das Admininterface
// Es können mehrere WLANs angegeben werden, mit denen sich der ESP verbinden soll.
char wifiSsid1[35] = "Fabmobil"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
char wifiPasswort1[35] = "NurFuerDieCoolenKids!"; // WLAN Passwort für das fremde Wifi
char wifiSsid2[35] = "Tommy"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
char wifiPasswort2[35] = "freibier"; // WLAN Passwort für das fremde Wifi
char wifiSsid3[35] = "Magrathea"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
char wifiPasswort3[35] = "Gemeinschaftskueche"; // WLAN Passwort für das fremde Wifi


#if MODUL_INFLUXDB
// InfluxDB v2 Authentifizierungstoken:
char influxToken[100] = "O24__XgbcJyoctWgsEjot6lW2Eh_xX-Jrw54cJ5YLssz8EIYAEd62Xgj_ulSeBeH4w-4o5PpLGbWeE7dpM8tcg==";
char influxOrganisation[35] = "<your org>";  // InfluxDB v2 Organisationsname
char influxBucket[35] = "<your bucket>"; //InfluxDB v2 Bucketname
char influxDatenbank[35] = "collectd"; // InfluxDB v1 Datenbankname
char influxBenutzer[35] = "collectd";  // InfluxDB v1 Benutzername
char influxPasswort[35] = "collectd"; // InfluxDB v1 Passwort
#endif
