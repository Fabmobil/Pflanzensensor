/**
 * Fabmobil Pflanzensensor Passwortdatei
 *
 * hier werden Passwörter gespeichert. Sie befinden sich in einer Extradatei damit sie nicht mit auf
 * github.com in das öffentliche Repository abgelegt werden.
 * Dies ist die Beispieldatei. Ändere die Passwörter entsprechend und nenne sie dann in passwoerter.h um.
 */

#if MODUL_WEBHOOK // wenn das Webhook Modul aktiviert ist
  String webhookDomain = "hook.eu2.make.com"; // vorderer Teil der Webhook-URL
  String webhookPfad = "/w0ynsd6suxxxxxxxxxxxxxxxxx3839dh"; // hinterer Teil der Webhook-URL
#endif

#if MODUL_WIFI // wenn das Wifimodul aktiv ist
  String wifiApPasswort = "geheim"; // Das Passwort für das selbst aufgemacht WLAN

  // Es können mehrere WLANs angegeben werden, mit denen sich der ESP verbinden soll. Ggfs. in allen Dateien
  // nach "wifiSsid1" suchen und die Zeilen kopieren und mehr hinzufügen:
  String wifiSsid1 = "WLAN1"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword1 = "Passwort1"; // WLAN Passwort für das fremde Wifi
  String wifiSsid2 = "WLAN2"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword2 = "Passwort2"; // WLAN Passwort für das fremde Wifi
  String wifiSsid3 = "WLAN3"; // WLAN Name / SSID wenn sich der ESP zu fremden Wifi verbinden soll
  String wifiPassword3 = "Passwort3"; // WLAN Passwort für das fremde Wifi
#endif
