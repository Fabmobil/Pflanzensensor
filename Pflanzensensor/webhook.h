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
#include <ESP8266HTTPClient.h>
#include "webhook_zertifikat.h"

X509List cert(makeComRootZertifikat);



void WebhookNachricht(int bodenfeuchte, int luftfeuchte, int lufttemperatur) {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von WebhookNachricht()"));
    Serial.print(F("# Bodenfeuchte: ")); Serial.print(bodenfeuchte);
    Serial.print(F(", Luftfeuchte: ")); Serial.print(luftfeuchte);
    Serial.print(F(", Lufttemperatur: ")); Serial.println(lufttemperatur);
  #endif

  // Stellt die https-Verbindung zum maker.com - Webhook Server her:
  WiFiClientSecure client;
  Serial.println("Verbinde zum Webhook-Server " + webhookDomain);
  Serial.printf("Mit dem Zertifikat: %s\n", makeComRootZertifikat);
  client.setTrustAnchors(&cert);
  if (!client.connect(webhookDomain, 443)) {
      Serial.println("Verbindung fehlgeschlagen!");
      return;
  }
  Serial.println("Verbindung erfolgreich!");

  // JSON Daten zusammenbauen:
  String jsonString = "";
  jsonString += "{\"bodenfeuchte:\":\"";
  jsonString += bodenfeuchte;
 // jsonString += "\",\"luftfeuchte\":\"";
 // jsonString += luftfeuchte;
 // jsonString += "\",\"lufttemperatur\":\"";
 // jsonString += lufttemperatur;
  jsonString += "\"}";
  int jsonLength = jsonString.length();
  String lenString = String(jsonLength);

  // POST-request zusammenbauen:
  String postString = "";
  postString += "POST ";
  postString += webhookPfad;
  postString += " HTTP/1.1\r\n";
  postString += "Host: ";
  postString += webhookDomain;
  postString += "\r\n";
  postString += "Content-Type: application/json\r\n";
  postString += "Content-Length: ";
  postString += lenString + "\r\n";
  postString += "\r\n";
  postString += jsonString; // combine post request and JSON

  client.print(postString);
  delay(500);
  client.stop();
  Serial.println(postString);
}
