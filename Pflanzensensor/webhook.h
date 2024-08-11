#include <WiFiClientSecure.h>
#include <time.h>
#include <ArduinoJson.h>
#include "webhook_zertifikat.h"

const int httpsPort = 443;
// Globale Variablen für Zertifikate
X509List certList;
WiFiClientSecure client;

// Funktionsdeklarationen
void WebhookSetup();
void verbindungTest();
void WebhookNachricht(String status, String sensorname, int sensorwert, String einheit);


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
  Serial.print("Warte auf die Synchronisation von Uhrzeit und Datum: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Die Zeit und das Datum ist: ");
  Serial.println(asctime(&timeinfo));

  // Zertifikate initialisieren
  certList.append(zertifikat);
  client.setTrustAnchors(&certList);
  WebhookNachricht("init", F("null"), 0 , F("null")); // Initalisierungsnachricht schicken
  // Testverbindung
  #if MODUL_DEBUG
    verbindungTest();
  #endif
}

/*
 * Funktion: verbindungTest()
 * Testet die Verbindung zum Server
 */
void verbindungTest() {
  Serial.println(F("# Testverbindung zum make.com Server:"));
  Serial.print("# Verbinde mit ");
  Serial.println(webhookDomain);
  Serial.printf("# Freier Heap: %d bytes\n", ESP.getFreeHeap());

  if (!client.connect(webhookDomain, httpsPort)) {
    Serial.println("# Verbindung fehlgeschlagen");
    Serial.print("# Letzter Fehlercode: ");
    Serial.println(client.getLastSSLError());
    return;
  }

  // HTTP-Anfrage senden
  String url = webhookPfad;
  Serial.print("# Anfrage URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
              "Host: " + webhookDomain + "\r\n" +
              "User-Agent: ESP8266\r\n" +
              "Connection: close\r\n\r\n");

  Serial.println("# Anfrage gesendet");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("# Headers empfangen");
      break;
    }
  }
  String response = client.readString();
  Serial.println("# Antwort war:");
  Serial.println("==========");
  Serial.println(response);
  Serial.println("==========");
  Serial.println("# Schließe Verbindung");
}

/*
 * Funktion: WebhookNachricht(String status, int bodenfeuchte, int luftfeuchte, int lufttemperatur)
 * Sendet Nachrichten über einen www.ifttt.com Webhook
 */
void WebhookNachricht(String status, String sensorname, int sensorwert, String einheit) {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von Webhooknachricht("));
    Serial.print(status); Serial.print(F(", ")); Serial.print(sensorname);
    Serial.print(F(", ")); Serial.print(sensorwert);
    Serial.println(F(")"));
  #endif

  // JSON-Objekt erstellen
  JsonDocument doc;
  doc["status"] = status;
  doc["sensorname"] = sensorname;
  doc["sensorwert"] = sensorwert;
  doc["einheit"] = einheit;

  // JSON in String umwandeln
  String jsonString;
  serializeJson(doc, jsonString);

  // POST-Request erstellen
  String postRequest = String("POST ") + webhookPfad + " HTTP/1.1\r\n" +
                       "Host: " + webhookDomain + "\r\n" +
                       "Content-Type: application/json\r\n" +
                       "Content-Length: " + jsonString.length() + "\r\n" +
                       "\r\n" +
                       jsonString;

  // Verbindung herstellen und Request senden
  if (client.connect(webhookDomain, httpsPort)) {
    client.print(postRequest);
    // Warten auf Antwort
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
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
