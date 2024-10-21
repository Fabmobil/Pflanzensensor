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

  // Zeit synchronisieren
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  logger.info(F("Warte auf die Synchronisation von Uhrzeit und Datum: "));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    logger.debug(".");
    now = time(nullptr);
  }
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  logger.info(F("Die Zeit und das Datum ist: ") + String(asctime(&timeinfo)));

  // Zertifikate initialisieren
  certList.append(zertifikat);
  client.setTrustAnchors(&certList);
  logger.info(F("Schicke Initialisierungsnachricht an Webhook-Dienst."));
  WebhookSendeInit(); // Initalisierungsnachricht schicken
}

void WebhookSendeInit() {
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
  WebhookSendeDaten(jsonString);
}

void WebhookErfasseSensordaten(const char* statusWert) {
  JsonDocument dok;
  JsonArray sensorData = dok["sensorData"].to<JsonArray>();
  bool hatAktivenAlarm = false;

  // Lambda-Funktion zum Hinzufügen von Sensorinformationen
  auto fuegeHinzuSensorInfo = [&](float wert, const String& name, const char* einheit, const String& status, bool alarmAktiv) {
    if (alarmAktiv) {
      JsonObject sensorObj = sensorData.add<JsonObject>();
      sensorObj["name"] = name;
      sensorObj["wert"] = wert;
      sensorObj["einheit"] = einheit;
      sensorObj["status"] = status;
      if (status == "rot") {
        hatAktivenAlarm = true;
      }
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

  // Setze den webhookStatus
  if (strcmp(statusWert, "ping") == 0) {
    dok["status"] = "ping";
  } else {
    dok["status"] = hatAktivenAlarm ? "Alarm" : "OK";
  }
  dok["alarmfrequenz"] = webhookFrequenz;
  dok["pingfrequenz"] = webhookPingFrequenz;

  // Serialisiere und sende die gesammelten Daten
  String jsonString;
  serializeJson(dok, jsonString);
  WebhookSendeDaten(jsonString);
}

void WebhookSendeDaten(const String& jsonString) {
  logger.info(F("Sende folgendes JSON an Webhook: "));
  logger.info(jsonString);
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
  } else {
    logger.error(F("Verbindung fehlgeschlagen"));
    logger.error(F("Letzter Fehlercode: "));
    logger.error(String(client.getLastSSLError()));
  }
  client.stop();
}

bool WebhookAktualisiereAlarmStatus() {
  bool aktuellerAlarm = false;

  #if MODUL_BODENFEUCHTE
    aktuellerAlarm |= (bodenfeuchteFarbe == "rot" && bodenfeuchteWebhook);
  #endif
  #if MODUL_HELLIGKEIT
    aktuellerAlarm |= (helligkeitFarbe == "rot" && helligkeitWebhook);
  #endif
  #if MODUL_DHT
    aktuellerAlarm |= ((luftfeuchteFarbe == "rot" && luftfeuchteWebhook) || (lufttemperaturFarbe == "rot" && lufttemperaturWebhook));
  #endif
  #if MODUL_ANALOG3
    aktuellerAlarm |= (analog3Farbe == "rot" && analog3Webhook);
  #endif
  #if MODUL_ANALOG4
    aktuellerAlarm |= (analog4Farbe == "rot" && analog4Webhook);
  #endif
  #if MODUL_ANALOG5
    aktuellerAlarm |= (analog5Farbe == "rot" && analog5Webhook);
  #endif
  #if MODUL_ANALOG6
    aktuellerAlarm |= (analog6Farbe == "rot" && analog6Webhook);
  #endif
  #if MODUL_ANALOG7
    aktuellerAlarm |= (analog7Farbe == "rot" && analog7Webhook);
  #endif
  #if MODUL_ANALOG8
    aktuellerAlarm |= (analog8Farbe == "rot" && analog8Webhook);
  #endif

  return aktuellerAlarm;
}
