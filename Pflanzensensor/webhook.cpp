#include "einstellungen.h"
#include "webhook.h"
#include "webhook_zertifikat.h"
#include "logger.h"
#include <time.h>

bool vorherAlarm = false;
String letzterWebhookStatus = "OK";
String webhookStatus = "init";
const int httpsPort = 443;

// Globale Variablen für Zertifikate
X509List certList;
WiFiClientSecure client;

void WebhookSetup() {
  logger.debug(F("Beginn von WebhookSetup()"));

  // Warte auf eine stabile WLAN-Verbindung
  int versuche = 0;
  while (WiFi.status() != WL_CONNECTED && versuche < 10) {
    logger.info(F("Warte auf stabile WLAN-Verbindung..."));
    delay(1000);
    versuche++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    logger.error(F("Konnte keine stabile WLAN-Verbindung herstellen. Webhook-Setup abgebrochen."));
    return;
  }

  // Zertifikate initialisieren
  certList.append(zertifikat);
  client.setTrustAnchors(&certList);

  logger.info(F("Schicke Initialisierungsnachricht an Webhook-Dienst."));

  // Wiederhole den Versuch, falls er fehlschlägt
  for (int i = 0; i < 3; i++) {
    if (WebhookSendeInit()) {
      logger.info(F("Webhook erfolgreich initialisiert."));
      return;
    }
    logger.warning(F("Webhook-Initialisierung fehlgeschlagen. Versuche es erneut..."));
    delay(2000);
  }

  logger.error(F("Webhook-Initialisierung nach 3 Versuchen fehlgeschlagen."));
}
bool WebhookSendeInit() {
  logger.updateNTP();
  logger.debug(F("Beginn von WebhookSendeInit()"));

  // JSON-Objekt erstellen
  JsonDocument doc;
  JsonArray gruenArray = doc["gruen"].to<JsonArray>();
  JsonObject gruenObj = gruenArray.add<JsonObject>();
  gruenObj["name"] = "Neustarts";
  gruenObj["wert"] = neustarts;
  gruenObj["einheit"] = "";
  doc["status"] = "init";
  doc["alarmfrequenz"] = webhookFrequenz;
  doc["pingfrequenz"] = webhookPingFrequenz;
  // JSON in String umwandeln
  String jsonString;
  serializeJson(doc, jsonString);
  bool result = WebhookSendeDaten(jsonString);
  return result;
}

void WebhookErfasseSensordaten(const char* statusWert) {
  JsonDocument dok;
  JsonArray gruenArray = dok["gruen"].to<JsonArray>();
  JsonArray gelbArray = dok["gelb"].to<JsonArray>();
  JsonArray rotArray = dok["rot"].to<JsonArray>();
  bool hatAktivenAlarm = false;

  // Funktion zum Hinzufügen von Sensorinformationen zur entsprechenden Kategorie
  auto fuegeHinzuSensorInfo = [&](int wert, const String& name, const char* einheit, const String& status, bool alarmAktiv) {
    JsonArray& zielArray = (status == "gruen") ? gruenArray : (status == "gelb" ? gelbArray : rotArray);
    JsonObject sensorObj = zielArray.add<JsonObject>();
    sensorObj["name"] = name;
    sensorObj["wert"] = wert;
    sensorObj["einheit"] = einheit;
    if (status == "rot" && alarmAktiv) {
      hatAktivenAlarm = true;
    }
  };

  // Füge Sensorinformationen hinzu
  #if MODUL_BODENFEUCHTE
    fuegeHinzuSensorInfo(bodenfeuchteMesswertProzent, bodenfeuchteName, "%", bodenfeuchteFarbe, bodenfeuchteWebhook);
  #endif
  #if MODUL_HELLIGKEIT
    fuegeHinzuSensorInfo(helligkeitMesswertProzent, helligkeitName, "%", helligkeitFarbe, helligkeitWebhook);
  #endif
  #if MODUL_DHT
    fuegeHinzuSensorInfo(luftfeuchteMesswert, "Luftfeuchte", "%", luftfeuchteFarbe, luftfeuchteWebhook);
    fuegeHinzuSensorInfo(lufttemperaturMesswert, "Lufttemperatur", "°C", lufttemperaturFarbe, lufttemperaturWebhook);
  #endif
  #if MODUL_ANALOG3
    fuegeHinzuSensorInfo(analog3MesswertProzent, analog3Name, "%", analog3Farbe, analog3Webhook);
  #endif
  #if MODUL_ANALOG4
    fuegeHinzuSensorInfo(analog4MesswertProzent, analog4Name, "%", analog4Farbe, analog4Webhook);
  #endif
  #if MODUL_ANALOG5
    fuegeHinzuSensorInfo(analog5MesswertProzent, analog5Name, "%", analog5Farbe, analog5Webhook);
  #endif
  #if MODUL_ANALOG6
    fuegeHinzuSensorInfo(analog6MesswertProzent, analog6Name, "%", analog6Farbe, analog6Webhook);
  #endif
  #if MODUL_ANALOG7
    fuegeHinzuSensorInfo(analog7MesswertProzent, analog7Name, "%", analog7Farbe, analog7Webhook);
  #endif
  #if MODUL_ANALOG8
    fuegeHinzuSensorInfo(analog8MesswertProzent, analog8Name, "%", analog8Farbe, analog8Webhook);
  #endif

  // Setze webhookStatus
  if (strcmp(statusWert, "ping") == 0) {
    webhookStatus = "ping";
  } else {
    webhookStatus = hatAktivenAlarm ? "Alarm" : "OK";
  }
  dok["status"] = webhookStatus;
  dok["alarmfrequenz"] = webhookFrequenz;
  dok["pingfrequenz"] = webhookPingFrequenz;

  // Sende die gesammelten Daten
  String jsonString;
  serializeJson(dok, jsonString);
  WebhookSendeDaten(jsonString);
}

bool WebhookSendeDaten(const String& jsonString) {
  // POST-Anfrage erstellen
  String postAnfrage = String("POST ") + webhookPfad + " HTTP/1.1\r\n" +
                       "Host: " + webhookDomain + "\r\n" +
                       "Content-Type: application/json\r\n" +
                       "Content-Length: " + jsonString.length() + "\r\n" +
                       "\r\n" +
                       jsonString;

  // Verbindung herstellen und Anfrage senden
  if (client.connect(webhookDomain, httpsPort)) {
    client.print(postAnfrage);
    // Warten auf Antwort
    while (client.connected()) {
      String zeile = client.readStringUntil('\n');
      if (zeile == "\r") {
        break;
      }
    }
    client.stop();
    return true;
  } else {
    logger.error(F("Verbindung fehlgeschlagen"));
    logger.error(F("Letzter Fehlercode: "));
    logger.error(String(client.getLastSSLError()));
    return false;
  }
}

bool WebhookAktualisiereAlarmStatus() {
  bool aktuellerAlarm = false;

  #if MODUL_BODENFEUCHTE
    aktuellerAlarm |= (strcmp(bodenfeuchteFarbe, "rot") == 0 && bodenfeuchteWebhook);
  #endif
  #if MODUL_HELLIGKEIT
    aktuellerAlarm |= (strcmp(helligkeitFarbe, "rot") == 0 && helligkeitWebhook);
  #endif
  #if MODUL_DHT
    aktuellerAlarm |= ((strcmp(luftfeuchteFarbe, "rot") == 0 && luftfeuchteWebhook) || (strcmp(lufttemperaturFarbe, "rot") == 0 && lufttemperaturWebhook));
  #endif
  #if MODUL_ANALOG3
    aktuellerAlarm |= (strcmp(analog3Farbe, "rot") == 0 && analog3Webhook);
  #endif
  #if MODUL_ANALOG4
    aktuellerAlarm |= (strcmp(analog4Farbe, "rot") == 0 && analog4Webhook);
  #endif
  #if MODUL_ANALOG5
    aktuellerAlarm |= (strcmp(analog5Farbe, "rot") == 0 && analog5Webhook);
  #endif
  #if MODUL_ANALOG6
    aktuellerAlarm |= (strcmp(analog6Farbe, "rot") == 0 && analog6Webhook);
  #endif
  #if MODUL_ANALOG7
    aktuellerAlarm |= (strcmp(analog7Farbe, "rot") == 0 && analog7Webhook);
  #endif
  #if MODUL_ANALOG8
    aktuellerAlarm |= (strcmp(analog8Farbe, "rot") == 0 && analog8Webhook);
  #endif

  return aktuellerAlarm;
}
