/*
 * DHT Modul
 * Dieses Modul enthält den Code für den DHT Luftfeuchte- und Lufttemperatursensor
 */

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

DHT_Unified dht(pinDht, dhtSensortyp);

/*
 * Funktion: DhtMessenLuftfeuchte()
 * Misst die Luftfeuchte in Prozent und gibt sie als Integer zurück
 */
int DhtMessenLuftfeuchte() {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von DhtMessenLuftfeuchte()"));
    Serial.print(F("DHT PIN: ")); Serial.println(pinDht);
    Serial.print(F("DHT Sensortyp: ")); Serial.println(dhtSensortyp);
    Serial.println(F("#######################################"));
  #endif
  sensors_event_t event;
  int luftfeuchte = -1;
  dht.humidity().getEvent(&event);
  // überprüfen ob die Messung erfolgreich war:
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Luftfeuchtemessung nicht erfolgreich! :-("));
  }
  else {
    Serial.print(F("Luftfeuchte: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
    luftfeuchte = event.relative_humidity;
  }
  #if MODUL_DEBUG
    Serial.println(F("#######################################"));
  #endif
  return luftfeuchte;
}


/**
 * Funktion: DhtMessenLufttemperatur()
 * Misst die Lufttemperatur in °C und gibt sie als Integer zurück
 */
int DhtMessenLufttemperatur() {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von DhtMessenLufttemperatur()"));
    Serial.print(F("DHT PIN: ")); Serial.println(pinDht);
    Serial.print(F("DHT Sensortyp: ")); Serial.println(dhtSensortyp);
  #endif
  sensors_event_t event;
  int lufttemperatur = -1;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.print(F("Temperaturmessung nicht erfolgreich! :-("));
  }
  else {
    Serial.print(F("Temperatur: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
    lufttemperatur = event.temperature;
  }
  #if MODUL_DEBUG
    Serial.println(F("#######################################"));
  #endif
  return lufttemperatur;
}
