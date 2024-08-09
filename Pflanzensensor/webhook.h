/**
 * Wifi Modul
 * Diese Datei enthält den Code für das zapier Webhook Modul
 * www.zapier.com ist ein Webservice der es ermöglicht, dass der Pflanzensensor dir Emails oder Telegramnachrichten schickt.
 *
 * curl test:  curl -X POST https://hook.eu2.make.com/w0ynsd6suxkrls7s6bgr8r5s8m3839dh -H "Content-Type: application/json" -d '{"bodenfeuchte":"20","luftfeuchte":"32","lufttemperatur":"30"}'
 */

/*
 * Funktion: WebhookNachricht(int bodenfeuchte, int luftfeuchte, int lufttemperatur)
 * Sendet Nachrichten über einen www.ifttt.com Webhook
 * bodenfeuchte: Bodenfeuchte in %
 * helligkeit: Helligkeit in %
 * luftfeuchte: Luftfeuchte in %
 * lufttemperatur: Lufttemperatur in °C
 */
#include <WiFiClientSecure.h>
//#include <ESP8266HTTPClient.h>
#include <time.h>
#include "webhook_zertifikat.h"

// Make.com API-Informationen
const char* host = "hook.eu2.make.com";
const int httpsPort = 443;

void WebhookSetup(){
  // Wir brauchen einen aktuellen Timestamp für https-Zertifikate
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
  Serial.print(asctime(&timeinfo));
  delay(1000);
  // HTTPS-Verbindung herstellen
  WiFiClientSecure client;
  X509List certList;
  certList.append(zertifikat);
  client.setTrustAnchors(&certList);
  Serial.print("Verbinde mit ");
  Serial.println(host);
  Serial.printf("Freier Heap: %d bytes\n", ESP.getFreeHeap());
  if (!client.connect(host, httpsPort)) {
    Serial.println("Verbindung fehlgeschlagen");
    Serial.print("Letzter Fehlercode: ");
    Serial.println(client.getLastSSLError());
    return;
  }

  // HTTP-Anfrage senden
  String url = webhookPfad;
  Serial.print("Anfrage URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
              "Host: " + host + "\r\n" +
              "User-Agent: ESP8266\r\n" +
              "Connection: close\r\n\r\n");

  Serial.println("Anfrage gesendet");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers empfangen");
      break;
    }
  }
  String response = client.readString();
  Serial.println("Antwort war:");
  Serial.println("==========");
  Serial.println(response);
  Serial.println("==========");
  Serial.println("Schließe Verbindung");
}

void WebhookNachricht(int bodenfeuchte, int luftfeuchte, int lufttemperatur){
  // // Stellt die https-Verbindung zum maker.com - Webhook Server her:
  // String zertifikat = zertifikatLaden();
  // if (zertifikat.length() == 0) {
  //   Serial.println(F("Zertifikat konnte nicht geladen werden"));
  //   return;
  // }
  // WiFiClientSecure client;
  // X509List certList;
  // certList.append(zertifikat.c_str());
  // client.setTrustAnchors(&certList);

  // Serial.println(F("Verbinde zum Webhook-Server ") + webhookDomain);

  // if (!client.connect(webhookDomain, 443)) {
  //   Serial.println("Verbindung fehlgeschlagen");
  //   return;
  // }

  // // HTTP-Anfrage senden
  // String url = "/"; // Ersetzen Sie dies durch den spezifischen Pfad Ihres Webhooks
  // Serial.print("Anfrage URL: ");
  // Serial.println(url);

  // client.print(String("GET ") + url + " HTTP/1.1\r\n" +
  //              "Host: " + webhookDomain + "\r\n" +
  //              "User-Agent: ESP8266\r\n" +
  //              "Connection: close\r\n\r\n");

  // Serial.println("Anfrage gesendet");
  // while (client.connected()) {
  //   String line = client.readStringUntil('\n');
  //   if (line == "\r") {
  //     Serial.println("Headers empfangen");
  //     break;
  //   }
  // }
  // String response = client.readString();
  // Serial.println("Antwort war:");
  // Serial.println("==========");
  // Serial.println(response);
  // Serial.println("==========");
  // Serial.println("Schließe Verbindung");
}
