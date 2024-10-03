/**
 * @file influxdb.h
 * @brief InfluxDB Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-10-31
 *
 * Dieses Modul enthält Funktionen um Sensordaten zu einer InfluxDB-Datenbank zu schicken
 */

#ifndef INFLUXDB_H
#define INFLUXDB_H

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// je nachdem, ob InfluxDB V1 oder V2 verwendet wird, wird das Objekt unterschiedlich initialisiert:

// Globale Zeigervariable für den InfluxDBClient
InfluxDBClient* influxClient = nullptr;

Point sensor(wifiHostname);

void InfluxSetup() {
    if (influx2) {
        influxClient = new InfluxDBClient(influxServer, influxOrg, influxBucket, influxToken, InfluxDbCloud2CACert);
    } else {
        influxClient = new InfluxDBClient(influxServer, influxDatenbank);
        // Set InfluxDB 1 authentication params
        influxClient->setConnectionParamsV1(influxServer, influxDatenbank, influxUser, influxPasswort);
    }
    // Set tags
    sensor.addTag("device", wifiHostname);

    if (influxClient->validateConnection()) {
        Serial.print("Connected to InfluxDB: ");
        Serial.println(influxClient->getServerUrl());
    } else {
        Serial.print("InfluxDB connection failed: ");
        Serial.println(influxClient->getLastErrorMessage());
    }
}

void InfluxSendeDaten() {
    Serial.println(F("Sende Daten an InfluxDB.."));
    sensor.clearFields();
    // Report RSSI of currently connected network
    sensor.addField("rssi", WiFi.RSSI());
    sensor.addField("uptime", millis());
    sensor.addField("SSID", WiFi.SSID());
    sensor.addField("reboots", neustarts);
    #if MODUL_BODENFEUCHTE
        sensor.addField(bodenfeuchteName, bodenfeuchteMesswertProzent);
    #endif
    #if MODUL_DHT
        sensor.addField("Lufttemperatur", lufttemperaturMesswert);
        sensor.addField("Luftfeuchte", luftfeuchteMesswert);
    #endif
    #if MODUL_HELLIGKEIT
        sensor.addField("Helligkeit", helligkeitMesswertProzent);
    #endif
    #if MODUL_ANALOG3
        sensor.addField(analog3Name, analog3MesswertProzent);
    #endif
     #if MODUL_ANALOG4
        sensor.addField(analog4Name, analog4MesswertProzent);
    #endif
     #if MODUL_ANALOG5
        sensor.addField(analog5Name, analog5MesswertProzent);
    #endif
     #if MODUL_ANALOG6
        sensor.addField(analog6Name, analog6MesswertProzent);
    #endif
     #if MODUL_ANALOG7
        sensor.addField(analog7Name, analog7MesswertProzent);
    #endif
     #if MODUL_ANALOG8
        sensor.addField(analog8Name, analog8MesswertProzent);
    #endif
    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(influxClient->pointToLineProtocol(sensor));
    // Write point
    if (!influxClient->writePoint(sensor)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(influxClient->getLastErrorMessage());
    }
}
#endif
