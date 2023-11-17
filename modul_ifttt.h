/**
 * Wifi Modul
 * Diese Datei enthält den Code für das Wifi-Modul und den Webserver
 */

/*
 * Funktion: ifttt_nachricht(int bodenfeuchte, int helligkeit, int luftfeuchte, int lufttemperatur)
 * Sendet Nachrichten über einen www.ifttt.com Webhook
 * bodenfeuchte: Bodenfeuchte in %
 * helligkeit: Helligkeit in %
 * luftfeuchte: Luftfeuchte in %
 * lufttemperatur: Lufttemperatur in °C
 */
void ifttt_nachricht(int bodenfeuchte, int helligkeit, int luftfeuchte, int lufttemperatur) {
  // JSON Datei zusammenbauen:
  String jsonString = "";
  jsonString += "{\"bodenfeuchte:\":\"";
  jsonString += bodenfeuchte;
  jsonString += "\",\"helligkeit:\":\"";
  jsonString += helligkeit;
  jsonString += "\",\"luftfeuchte\":\"";
  jsonString += luftfeuchte;
  jsonString += "\",\"lufttemperatur\":\"";
  jsonString += lufttemperatur;
  jsonString += "\"}";
  int jsonLength = jsonString.length();
  String lenString = String(jsonLength);
  // connect to the Maker event server
  client.connect("maker.ifttt.com", 80);
  // construct the POST request
  String postString = "";
  postString += "POST /trigger/";
  postString += wifiIftttEreignis;
  postString += "/with/key/";
  postString += wifiIftttEreignis;
  postString += " HTTP/1.1\r\n";
  postString += "Host: maker.ifttt.com\r\n";
  postString += "Content-Type: application/json\r\n";
  postString += "Content-Length: ";
  postString += lenString + "\r\n";
  postString += "\r\n";
  postString += jsonString; // combine post request and JSON

  client.print(postString);
  delay(500);
  client.stop();
}
