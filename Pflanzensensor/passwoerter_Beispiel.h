/**
 * @file passwoerter.h
 * @brief Passwort- und Sicherheitskonfiguration für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält sensible Informationen wie Passwörter und Zugangsdaten.
 */

#ifndef PASSWOERTER_H
#define PASSWOERTER_H

#if MODUL_WEBHOOK // wenn das Webhook Modul aktiviert ist
  String webhookDomain = "hook.eu2.make.com"; // vorderer Teil der Webhook-URL
  String webhookPfad = "/w0ynsd6suxxxxxxxxxxxxxxxxx3839dh"; // hinterer Teil der Webhook-URL
#endif

#if MODUL_WIFI // wenn das Wifimodul aktiv ist
  bool wifiApPasswortAktiviert = false; // soll das selbst aufgemachte WLAN ein Passwort haben?
  String wifiApPasswort = "geheim"; // Das Passwort für das selbst aufgemacht WLAN
  String wifiAdminPasswort = "admin"; // Passwort für das Admininterface
  // Es können mehrere WLANs angegeben werden, mit denen sich der ESP verbinden soll. Ggfs. in allen Dateien
  // nach "wifiSsid1" suchen und die Zeilen kopieren und mehr hinzufügen:
  String wifiSsid1 = "WLAN1"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword1 = "Passwort1"; // WLAN Passwort für das fremde Wifi
  String wifiSsid2 = "WLAN2"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword2 = "Passwort2"; // WLAN Passwort für das fremde Wifi
  String wifiSsid3 = "WLAN3"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword3 = "Passwort3"; // WLAN Passwort für das fremde Wifi
#endif

#if MODUL_INFLUXDB
  // InfluxDB v2 Authentifizierungstoken:
  String influxToken = "Token";
  String influxOrganisation = "Organisationsname";  // InfluxDB v2 Organisationsname
  String influxBucket = "Bucket"; //InfluxDB v2 Bucketname
  String influxDatenbank = "Datenbankname"; // InfluxDB v1 Datenbankname
  String influxBenutzer = "Benutzername";  // InfluxDB v1 Benutzername
  String influxPasswort = "Passwort"; // InfluxDB v1 Passwort
#endif

#endif // PASSWOERTER_H
