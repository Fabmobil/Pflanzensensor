/**
 * @file variablenspeicher.cpp
 * @brief Implementierung der Funktionen zum Speichern und Laden von Variablen
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul implementiert Funktionen zum Speichern und Laden von Variablen
 * im Flash-Speicher des ESP8266.
 */

#include "variablenspeicher.h"
#include "einstellungen.h"
#include "passwoerter.h"
#if MODUL_DISPLAY
  #include "display.h"
#endif
Preferences variablen;

/**
 * @brief Überprüft, ob Variablen im Flash-Speicher vorhanden sind
 *
 * @return bool true wenn Variablen vorhanden sind, sonst false
 */
bool VariablenDa() {
  variablen.begin("pflanzensensor", true);
  bool variablenDa = variablen.getBool("variablenDa", false);
  return variablenDa;
}

/**
 * @brief Speichert alle Variablen im Flash-Speicher
 */
void VariablenSpeichern() {
  variablen.begin("pflanzensensor", false);
  variablen.putBool("variablenDa", true);
  variablen.putString("logLevel", logLevel);
  variablen.putInt("logAnzahlEintraege", logAnzahlEintraege);
  variablen.putInt("logAnzahlWebseite", logAnzahlWebseite);
  variablen.putBool("logInDatei", logInDatei);
  #if MODUL_DISPLAY
    variablen.putInt("intDisplay", intervallDisplay);
    variablen.putBool("displayAn", displayAn);
  #endif
  variablen.putInt("intBodenf", intervallAnalog);
  #if MODUL_BODENFEUCHTE
    variablen.putBool("bodenfWeb", bodenfeuchteWebhook);
    variablen.putString("bodenfName", bodenfeuchteName);
    variablen.putInt("bodenfGrUnten", bodenfeuchteGruenUnten);
    variablen.putInt("bodenfGrOben", bodenfeuchteGruenOben);
    variablen.putInt("bodenfGeUnten", bodenfeuchteGelbUnten);
    variablen.putInt("bodenfGeOben", bodenfeuchteGelbOben);
  #endif
  #if MODUL_ANALOG3
    variablen.putBool("analog3Web", analog3Webhook);
    variablen.putString("analog3Name", analog3Name);
    variablen.putInt("analog3GrUnten", analog3GruenUnten);
    variablen.putInt("analog3GrOben", analog3GruenOben);
    variablen.putInt("analog3GeUnten", analog3GelbUnten);
    variablen.putInt("analog3GeOben", analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    variablen.putBool("analog4Web", analog4Webhook);
    variablen.putString("analog4Name", analog4Name);
    variablen.putInt("analog4GrUnten", analog4GruenUnten);
    variablen.putInt("analog4GrOben", analog4GruenOben);
    variablen.putInt("analog4GeUnten", analog4GelbUnten);
    variablen.putInt("analog4GeOben", analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    variablen.putBool("analog5Web", analog5Webhook);
    variablen.putString("analog5Name", analog5Name);
    variablen.putInt("analog5GrUnten", analog5GruenUnten);
    variablen.putInt("analog5GrOben", analog5GruenOben);
    variablen.putInt("analog5GeUnten", analog5GelbUnten);
    variablen.putInt("analog5GeOben", analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    variablen.putBool("analog6Web", analog6Webhook);
    variablen.putString("analog6Name", analog6Name);
    variablen.putInt("analog6GrUnten", analog6GruenUnten);
    variablen.putInt("analog6GrOben", analog6GruenOben);
    variablen.putInt("analog6GeUnten", analog6GelbUnten);
    variablen.putInt("analog6GeOben", analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    variablen.putBool("analog7Web", analog7Webhook);
    variablen.putString("analog7Name", analog7Name);
    variablen.putInt("analog7GrUnten", analog7GruenUnten);
    variablen.putInt("analog7GrOben", analog7GruenOben);
    variablen.putInt("analog7GeUnten", analog7GelbUnten);
    variablen.putInt("analog7GeOben", analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    variablen.putBool("analog8Web", analog8Webhook);
    variablen.putString("analog8Name", analog8Name);
    variablen.putInt("analog8GrUnten", analog8GruenUnten);
    variablen.putInt("analog8GrOben", analog8GruenOben);
    variablen.putInt("analog8GeUnten", analog8GelbUnten);
    variablen.putInt("analog8GeOben", analog8GelbOben);
  #endif
  #if MODUL_DHT
    variablen.putInt("intDht", intervallDht);
    variablen.putBool("luftTWeb", lufttemperaturWebhook);
    variablen.putInt("luftTGrUnten", lufttemperaturGruenUnten);
    variablen.putInt("luftTGrOben", lufttemperaturGruenOben);
    variablen.putInt("luffTGeUnten", lufttemperaturGelbUnten);
    variablen.putInt("luftTGeOben", lufttemperaturGelbOben);
    variablen.putBool("luftFWeb", luftfeuchteWebhook);
    variablen.putInt("luftFGrUnten", luftfeuchteGruenUnten);
    variablen.putInt("luftFGrOben", luftfeuchteGruenOben);
    variablen.putInt("luftFGeUnten", luftfeuchteGelbUnten);
    variablen.putInt("luftFGeOben", luftfeuchteGelbOben);
  #endif
  #if MODUL_HELLIGKEIT
    variablen.putString("hellName", helligkeitName);
    variablen.putBool("hellWeb", helligkeitWebhook);
    variablen.putInt("hellMin", helligkeitMinimum);
    variablen.putInt("hellMax", helligkeitMaximum);
    variablen.putInt("hellGrUnten", helligkeitGruenUnten);
    variablen.putInt("hellGrOben", helligkeitGruenOben);
    variablen.putInt("hellGeUnten", helligkeitGelbUnten);
    variablen.putInt("hellGeOben", helligkeitGelbOben);
  #endif
  #if MODUL_LEDAMPEL
    variablen.putBool("ampelAn", ampelAn);
    variablen.putInt("ampelModus", ampelModus);
  #endif
  #if MODUL_WEBHOOK
    variablen.putString("webhookPfad", webhookPfad);
    variablen.putString("webhookDomain", webhookDomain);
    variablen.putInt("webhookFrequenz", webhookFrequenz);
    variablen.putInt("webhookPingFrequenz", webhookPingFrequenz);
    variablen.putBool("webhookAn", webhookAn);
  #endif
  #if MODUL_WIFI
    variablen.putString("adminPw", wifiAdminPasswort);
    variablen.putString("hostname", wifiHostname);
    variablen.putBool("apAktiv", wifiAp);
    variablen.putString("apSsid", wifiApSsid);
    variablen.putBool("apPwAktiv", wifiApPasswortAktiviert);
    variablen.putString("apPw", wifiApPasswort);
    variablen.putString("wifiSsid1", wifiSsid1);
    variablen.putString("wifiPw1", wifiPasswort1);
    variablen.putString("wifiSsid2", wifiSsid2);
    variablen.putString("wifiPw2", wifiPasswort2);
    variablen.putString("wifiSsid3", wifiSsid3);
    variablen.putString("wifiPw3", wifiPasswort3);
  #endif
  variablen.end();
}

/**
 * @brief Lädt alle Variablen aus dem Flash-Speicher
 */
void VariablenLaden() {
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    DisplayDreiWoerter("Start..", " Variablen", "  laden");
  #endif
  variablen.begin("pflanzensensor", true);
  String tempString = variablen.getString("logLevel", logLevel);
  strncpy(logLevel, tempString.c_str(), sizeof(logLevel) - 1);
  logLevel[sizeof(logLevel) - 1] = '\0';
  logAnzahlEintraege = variablen.getInt("logAnzahlEintraege", logAnzahlEintraege);
  logAnzahlWebseite = variablen.getInt("logAnzahlWebseite", logAnzahlWebseite);
  logInDatei = variablen.getBool("logInDatei", logInDatei);
  // Load the variables from flash
  #if MODUL_DISPLAY
    intervallDisplay = variablen.getInt("intDisplay", intervallDisplay);
    displayAn = variablen.getInt("displayAn", displayAn);
  #endif
  intervallAnalog = variablen.getInt("intAnalog", intervallAnalog);
  #if MODUL_BODENFEUCHTE
    bodenfeuchteWebhook = variablen.getInt("bodenfWeb", bodenfeuchteWebhook);
    bodenfeuchteGruenUnten = variablen.getInt("bodenfGrUnten", bodenfeuchteGruenUnten);
    bodenfeuchteGruenOben = variablen.getInt("bodenfGrOben", bodenfeuchteGruenOben);
    bodenfeuchteGelbUnten = variablen.getInt("bodenfGeUnten", bodenfeuchteGelbUnten);
    bodenfeuchteGelbOben = variablen.getInt("bodenfGeOben", bodenfeuchteGelbOben);
  #endif
  #if MODUL_ANALOG3
    tempString = variablen.getString("analog3Name", analog3Name);
    strncpy(analog3Name, tempString.c_str(), sizeof(analog3Name) - 1);
    analog3Name[sizeof(analog3Name) - 1] = '\0';
    analog3Webhook = variablen.getInt("analog3Web", analog3Webhook);
    analog3GruenUnten = variablen.getInt("analog3GrUnten", analog3GruenUnten);
    analog3GruenOben = variablen.getInt("analog3GrOben", analog3GruenOben);
    analog3GelbUnten = variablen.getInt("analog3GeUnten", analog3GelbUnten);
    analog3GelbOben = variablen.getInt("analog3GeOben", analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    tempString = variablen.getString("analog4Name", analog4Name);
    strncpy(analog4Name, tempString.c_str(), sizeof(analog4Name) - 1);
    analog4Name[sizeof(analog4Name) - 1] = '\0';
    analog4Webhook = variablen.getInt("analog4Web", analog4Webhook);
    analog4GruenUnten = variablen.getInt("analog4GrUnten", analog4GruenUnten);
    analog4GruenOben = variablen.getInt("analog4GrOben", analog4GruenOben);
    analog4GelbUnten = variablen.getInt("analog4GeUnten", analog4GelbUnten);
    analog4GelbOben = variablen.getInt("analog4GeOben", analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    tempString = variablen.getString("analog5Name", analog5Name);
    strncpy(analog5Name, tempString.c_str(), sizeof(analog5Name) - 1);
    analog5Name[sizeof(analog5Name) - 1] = '\0';
    analog5Webhook = variablen.getInt("analog5Web", analog5Webhook);
    analog5GruenUnten = variablen.getInt("analog5GrUnten", analog5GruenUnten);
    analog5GruenOben = variablen.getInt("analog5GrOben", analog5GruenOben);
    analog5GelbUnten = variablen.getInt("analog5GeUnten", analog5GelbUnten);
    analog5GelbOben = variablen.getInt("analog5GeOben", analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    tempString = variablen.getString("analog6Name", analog6Name);
    strncpy(analog6Name, tempString.c_str(), sizeof(analog6Name) - 1);
    analog6Name[sizeof(analog6Name) - 1] = '\0';
    analog6Webhook = variablen.getInt("analog6Web", analog6Webhook);
    analog6GruenUnten = variablen.getInt("analog6GrUnten", analog6GruenUnten);
    analog6GruenOben = variablen.getInt("analog6GrOben", analog6GruenOben);
    analog6GelbUnten = variablen.getInt("analog6GeUnten", analog6GelbUnten);
    analog6GelbOben = variablen.getInt("analog6GeOben", analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    tempString = variablen.getString("analog7Name", analog7Name);
    strncpy(analog7Name, tempString.c_str(), sizeof(analog7Name) - 1);
    analog7Name[sizeof(analog7Name) - 1] = '\0';
    analog7Webhook = variablen.getInt("analog7Web", analog7Webhook);
    analog7GruenUnten = variablen.getInt("analog7GrUnten", analog7GruenUnten);
    analog7GruenOben = variablen.getInt("analog7GrOben", analog7GruenOben);
    analog7GelbUnten = variablen.getInt("analog7GeUnten", analog7GelbUnten);
    analog7GelbOben = variablen.getInt("analog7GeOben", analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    tempString = variablen.getString("analog8Name", analog8Name);
    strncpy(analog8Name, tempString.c_str(), sizeof(analog8Name) - 1);
    analog8Name[sizeof(analog8Name) - 1] = '\0';
    analog8Webhook = variablen.getInt("analog8Web", analog8Webhook);
    analog8GruenUnten = variablen.getInt("analog8GrUnten", analog8GruenUnten);
    analog8GruenOben = variablen.getInt("analog8GrOben", analog8GruenOben);
    analog8GelbUnten = variablen.getInt("analog8GeUnten", analog8GelbUnten);
    analog8GelbOben = variablen.getInt("analog8GeOben", analog8GelbOben);
  #endif
  #if MODUL_DHT
    intervallDht = variablen.getInt("intDht", intervallDht);
    lufttemperaturWebhook = variablen.getInt("luftTWeb", lufttemperaturWebhook);
    lufttemperaturGruenUnten = variablen.getInt("luftTGrUnten", lufttemperaturGruenUnten);
    lufttemperaturGruenOben = variablen.getInt("luftTGrOben", lufttemperaturGruenOben);
    lufttemperaturGelbUnten = variablen.getInt("luffTGeUnten", lufttemperaturGelbUnten);
    lufttemperaturGelbOben = variablen.getInt("luftTGeOben", lufttemperaturGelbOben);
    luftfeuchteGruenUnten = variablen.getInt("luftFGrUnten", luftfeuchteGruenUnten);
    luftfeuchteWebhook = variablen.getInt("luftFWeb", luftfeuchteWebhook);
    luftfeuchteGruenOben = variablen.getInt("luftFGrOben", luftfeuchteGruenOben);
    luftfeuchteGelbUnten = variablen.getInt("luftFGeUnten", luftfeuchteGelbUnten);
    luftfeuchteGelbOben = variablen.getInt("luftFGeOben", luftfeuchteGelbOben);
  #endif
  #if MODUL_HELLIGKEIT
    tempString = variablen.getString("hellName", helligkeitName);
    strncpy(helligkeitName, tempString.c_str(), sizeof(helligkeitName) - 1);
    helligkeitName[sizeof(helligkeitName) - 1] = '\0';
    helligkeitWebhook = variablen.getInt("hellWeb", helligkeitWebhook);
    helligkeitMinimum = variablen.getInt("hellMin", helligkeitMinimum);
    helligkeitMaximum = variablen.getInt("hellMax", helligkeitMaximum);
    helligkeitGruenUnten = variablen.getInt("hellGrUnten", helligkeitGruenUnten);
    helligkeitGruenOben = variablen.getInt("hellGrOben", helligkeitGruenOben);
    helligkeitGelbUnten = variablen.getInt("hellGeUnten", helligkeitGelbUnten);
    helligkeitGelbOben = variablen.getInt("hellGeOben", helligkeitGelbOben);
  #endif
  #if MODUL_LEDAMPEL
    ampelAn = variablen.getBool("ampelAn", ampelAn);
    ampelModus = variablen.getInt("ampelModus", ampelModus);
  #endif
  #if MODUL_WEBHOOK
    tempString = variablen.getString("webhookPfad", webhookPfad);
    strncpy(webhookPfad, tempString.c_str(), sizeof(webhookPfad) - 1);
    webhookPfad[sizeof(webhookPfad) - 1] = '\0';
    tempString = variablen.getString("webhookDomain", webhookDomain);
    strncpy(webhookDomain, tempString.c_str(), sizeof(webhookDomain) - 1);
    webhookDomain[sizeof(webhookDomain) - 1] = '\0';
    webhookAn = variablen.getBool("webhookAn", webhookAn);
    webhookFrequenz = variablen.getInt("webhookFrequenz", webhookFrequenz);
    webhookPingFrequenz = variablen.getInt("webhookPingFrequenz", webhookPingFrequenz);
  #endif
  #if MODUL_WIFI
    tempString = variablen.getString("wifiSsid1", wifiSsid1);
    strncpy(wifiSsid1, tempString.c_str(), sizeof(wifiSsid1) - 1);
    wifiSsid1[sizeof(wifiSsid1) - 1] = '\0';
    tempString = variablen.getString("wifiPw1", wifiPasswort1);
    strncpy(wifiPasswort1, tempString.c_str(), sizeof(wifiPasswort1) - 1);
    wifiPasswort1[sizeof(wifiPasswort1) - 1] = '\0';
    tempString = variablen.getString("wifiSsid2", wifiSsid2);
    strncpy(wifiSsid2, tempString.c_str(), sizeof(wifiSsid2) - 1);
    wifiSsid2[sizeof(wifiSsid2) - 1] = '\0';
    tempString = variablen.getString("wifiPw2", wifiPasswort2);
    strncpy(wifiPasswort2, tempString.c_str(), sizeof(wifiPasswort2) - 1);
    wifiPasswort2[sizeof(wifiPasswort2) - 1] = '\0';
    tempString = variablen.getString("wifiSsid3", wifiSsid3);
    strncpy(wifiSsid3, tempString.c_str(), sizeof(wifiSsid3) - 1);
    wifiSsid3[sizeof(wifiSsid3) - 1] = '\0';
    tempString = variablen.getString("wifiPw3", wifiPasswort3);
    strncpy(wifiPasswort3, tempString.c_str(), sizeof(wifiPasswort3) - 1);
    wifiPasswort3[sizeof(wifiPasswort3) - 1] = '\0';
    tempString = variablen.getString("apSsid", wifiApSsid);
    strncpy(wifiApSsid, tempString.c_str(), sizeof(wifiApSsid) - 1);
    wifiApSsid[sizeof(wifiApSsid) - 1] = '\0';
    wifiApPasswortAktiviert = variablen.getBool("apPwAktiv", wifiApPasswortAktiviert);
    tempString = variablen.getString("apPw", wifiApPasswort);
    strncpy(wifiApPasswort, tempString.c_str(), sizeof(wifiApPasswort) - 1);
    wifiApPasswort[sizeof(wifiApPasswort) - 1] = '\0';
  #endif
  variablen.end();
}


/**
 * @brief Löscht alle gespeicherten Variablen im Flash-Speicher
 */
void VariablenLoeschen() {
  variablen.begin("pflanzensensor", false);
  variablen.clear();
  variablen.end();
}

/**
 * @brief Listet rekursiv alle Dateien und Verzeichnisse im Dateisystem auf
 *
 * @param dir Das zu durchsuchende Verzeichnis
 * @param numTabs Anzahl der Tabs für die Einrückung (für rekursive Aufrufe)
 */
void VariablenAuflisten(File dir, int numTabs) {
  while (true) {
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i=0; i<numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      VariablenAuflisten(entry, numTabs+1);
    } else {
    // files have sizes, directories do not
    Serial.print("\t\t");
    Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
