/**
 * Webhook Modul
 * Diese Datei enthält den Code für das make.com Webhook Modul
 * www.make.com ist ein Webservice der es ermöglicht, dass der Pflanzensensor dir Emails oder Telegramnachrichten schickt.
 *
 * curl test:  curl -X POST https://hook.eu2.make.com/URL -H "Content-Type: application/json" -d '{"bodenfeuchte":"20","luftfeuchte":"32","lufttemperatur":"30"}'
 */

#include <WiFiClientSecure.h>
#include <time.h>
#include <ArduinoJson.h>
#include "webhook_zertifikat.h"

bool vorherAlarm = false;
String letzterWebhookStatus = "OK";
const int httpsPort = 443;
// Globale Variablen für Zertifikate
X509List certList;
WiFiClientSecure client;

// Funktionsdeklarationen
void WebhookSetup();
void WebhookSendeInit();
void WebhookErfasseSensordaten(const char* statusWert);
void WebhookSendeDaten(const String& jsonString);
bool WebhookAktualisiereAlarmStatus();

/*
* Funktion: WebhookSetup()
* Initialisiert das Webhook-Modul
*/
void WebhookSetup() {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebhookSetup()"));
  #endif

  // Zeit synchronisieren
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("Warte auf die Synchronisation von Uhrzeit und Datum: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Die Zeit und das Datum ist: "));
  Serial.println(asctime(&timeinfo));

  // Zertifikate initialisieren
  certList.append(zertifikat);
  client.setTrustAnchors(&certList);
  Serial.println(F("Schicke Initialisierungsnachricht an Webhook-Dienst."));
  WebhookSendeInit(); // Initalisierungsnachricht schicken
}

/*
* Funktion: WebhookSendeInit(String status, int bodenfeuchte, int luftfeuchte, int lufttemperatur)
* Sendet Nachrichten über einen www.ifttt.com Webhook
*/
void WebhookSendeInit() {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von WebhookSendeInit()"));
  #endif

  // JSON-Objekt erstellen
  JsonDocument doc;
  doc["status"] = "init";
  JsonArray gruenArray = doc["gruen"].to<JsonArray>();
  JsonObject gruenObj = gruenArray.add<JsonObject>();
  gruenObj["name"] = "Neustarts";
  gruenObj["wert"] = neustarts;
  gruenObj["einheit"] = "";
  JsonArray gelbArray = doc["gelb"].to<JsonArray>();
  JsonObject gelbObj = gelbArray.add<JsonObject>();
  gelbObj["name"] = "Pingfrequenz";
  gelbObj["wert"] = webhookPingFrequenz;
  gelbObj["einheit"] = "Stunden";
  JsonArray rotArray = doc["rot"].to<JsonArray>();
  JsonObject rotObj = rotArray.add<JsonObject>();
  rotObj["name"] = "Alarmfrequenz";
  rotObj["wert"] = webhookFrequenz;
  rotObj["einheit"] = "Stunden";

  // JSON in String umwandeln
  String jsonString;
  serializeJson(doc, jsonString);
  WebhookSendeDaten(jsonString);
}

// Angepasste WebhookErfasseSensordaten Funktion
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

  // Sende die gesammelten Daten
  String jsonString;
  serializeJson(dok, jsonString);
  WebhookSendeDaten(jsonString);
}

void WebhookSendeDaten(const String& jsonString) {
  Serial.print(F("Sende folgendes JSON an Webhook: "));
  Serial.println(jsonString);
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
    Serial.println(F("Verbindung fehlgeschlagen"));
    Serial.print(F("Letzter Fehlercode: "));
    Serial.println(client.getLastSSLError());
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
