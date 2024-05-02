/**
 * Wifi Modul
 * Diese Datei enthält den Code für das zapier Webhook Modul
 * www.zapier.com ist ein Webservice der es ermöglicht, dass der Pflanzensensor dir Emails oder Telegramnachrichten schickt.
 *
 * curl test:  curl -X POST https://hook.eu2.make.com/w0ynsd6suxkrls7s6bgr8r5s8m3839dh -H "Content-Type: application/json" -d '{"bodenfeuchte":"20","luftfeuchte":"32","lufttemperatur":"30"}'
 */

/*
 * Funktion: webhook_nachricht(int bodenfeuchte, int luftfeuchte, int lufttemperatur)
 * Sendet Nachrichten über einen www.ifttt.com Webhook
 * bodenfeuchte: Bodenfeuchte in %
 * helligkeit: Helligkeit in %
 * luftfeuchte: Luftfeuchte in %
 * lufttemperatur: Lufttemperatur in °C
 */

void webhook_nachricht(int bodenfeuchte, int luftfeuchte, int lufttemperatur) {
  // JSON Datei zusammenbauen:
  String jsonString = "";
  jsonString += "{\"bodenfeuchte:\":\"";
  jsonString += bodenfeuchte;
  jsonString += "\",\"luftfeuchte\":\"";
  jsonString += luftfeuchte;
  jsonString += "\",\"lufttemperatur\":\"";
  jsonString += lufttemperatur;
  jsonString += "\"}";
  int jsonLength = jsonString.length();
  String lenString = String(jsonLength);
  // connect to the Maker event server
  Serial.println("\nVerbinde zum Webhook-Server...");
  if (!client.connect(webhookDomain, 443))
    Serial.println("Verbindung fehlgeschlagen!");
  else {
    Serial.println("Verbindung erfolgreich!");
    client.connect(webhookDomain, 443);
    // construct the POST request
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
}
