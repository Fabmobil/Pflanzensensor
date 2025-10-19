#ifndef CONFIG_EXAMPLE_H
#define CONFIG_EXAMPLE_H

/**
 * @file config_example.h
 * @brief Konfigurationsdatei für das ESP8266-basierte Sensorsystem
 */

// Geräteeinstellungen
#define DEVICE_NAME "Sensor Name"  /* Name Ihres Geräts */
#define FILE_LOGGING_ENABLED false /* Logging ins Dateisystem aktivieren */
#define MAX_LOG_FILE_SIZE 50000    /* Maximale Größe der Log-Datei in Bytes */
#define LOG_LEVEL "INFO"           /* Log-Level: INFO, DEBUG, ERROR, WARNING */

// Feature-Flags
// Feature-Flags
#define USE_DS18B20 true  // DS18B20 Temperatursensor. Mehrere können an einem PIN angeschlossen werden
#define USE_DHT true      // DHT11 oder DHT22 Temperatur- und Feuchtesensoren
#define USE_SDS011 true   // SDS011 Partikelmesssensor
#define USE_MHZ19 false   // MH-Z19 CO2-Messsensor
#define USE_ANALOG false  // Einen Analog-Sensor verwenden
#define USE_MULTIPLEXER false      // Mehrere Analogsensoren über Multiplexer verwenden
#define USE_SERIAL_RECEIVER false  // Durchflussempfänger-Sensor
#define USE_INFLUXDB true          // Daten an InfluxDB senden
#define USE_DISPLAY false          // Display verwenden
#define USE_LED_TRAFFIC_LIGHT false  // LED-Ampel verwenden
#define USE_WEBSERVER \
  true  // Webserver-Funktionalität verwenden (nur mit USE_WIFI)
#define USE_WEBSOCKET false  // WebSocket-Modul für Logs verwenden
#define USE_WIFI true        // WLAN des ESP verwenden

// Debug-Flags
#define DEBUG_RAM true /* RAM-Debugausgaben aktivieren */
#define DEBUG_MEASUREMENT_CYCLE \
  true                        /* Debugausgaben für Messzyklus aktivieren */
#define DEBUG_SENSOR true     /* Sensor-Debugausgaben aktivieren */
#define DEBUG_DISPLAY false   /* Display-Debugausgaben aktivieren */
#define DEBUG_WEBSOCKET false /* WebSocket-Debugausgaben aktivieren */

// Messeinstellungen
#define MEASUREMENT_INTERVAL 60  // in Sekunden
#define MEASUREMENT_DEINITIALIZE_SENSORS \
  false  // false: Sensoren aktiv lassen; \
                            // true: Sensoren nach jeder Messung deinitialisieren
#define MEASUREMENT_MINIMUM_DELAY \
  100  // minimale Verzögerung zwischen Messungen in Millisekunden
#define MEASUREMENT_AVERAGE_COUNT 3  // Anzahl Messungen für Mittelwertbildung
#define MEASUREMENT_ERROR_COUNT 3    // Maximale Fehler bis zur Neuinitialisierung des Sensors

// Netzwerkeinstellungen
#define WIFI_SSID_1 "WIFI SSID Name 1"    /* Primäre WLAN-SSID */
#define WIFI_PASSWORD_1 "WIFI password 1" /* Primäres WLAN-Passwort */
#define WIFI_SSID_2 ""     /* Sekundäre WLAN-SSID (optional) */
#define WIFI_PASSWORD_2 "" /* Sekundäres WLAN-Passwort (optional) */
#define WIFI_SSID_3 ""     /* Tertiäre WLAN-SSID (optional) */
#define WIFI_PASSWORD_3 "" /* Tertiäres WLAN-Passwort (optional) */
#define HOSTNAME "device hostname" /* Geräte-Hostname */
#define HOST_IP 172, 17, 1, 200    /* Host-IP-Adresse */
#define USE_STATIC_IP 0            /* 1: statische IP, 0: DHCP */
#define STATIC_IP HOST_IP          /* Statische IP-Konfiguration */
#define GATEWAY 172, 17, 1, 1      /* Router-IP-Adresse */
#define SUBNET 255, 255, 0, 0      /* Subnetzmaske */
#define PRIMARY_DNS 172, 17, 1, 1  /* Primärer DNS-Server */
#define SECONDARY_DNS 8, 8, 4, 4   /* Sekundärer DNS-Server */

// Webserver-Einstellungen
#define LOG_ENTRIES_TO_DISPLAY \
  20 /* Anzahl Log-Einträge auf der Weboberfläche anzeigen */
#define INITIAL_ADMIN_PASSWORD "admin123" /* Standard-Admin-Passwort */

// InfluxDB-Einstellungen
#define INFLUXDB_SYSTEMINFO_ACTIVE true /* Systeminformationen an InfluxDB senden */
#define INFLUXDB_SYSTEMINFO_INTERVAL \
  10 /* Intervall zum Senden von Systeminformationen in Minuten */
#define INFLUXDB_SEND_SINGLE_MEASUREMENT \
  false                                         // true: jede Messung einzeln senden; \
// false: alle Messungen zusammen senden
#define INFLUXDB_USE_V2 false                   /* false für V1, true für V2 */
#define INFLUXDB_URL "http://influxserver:8086" /* InfluxDB-Server-URL */
#define INFLUXDB_SENSORNAME "esp_sensor"        /* Sensorname in InfluxDB */
#define INFLUXDB_MEASUREMENT_NAME "sensorname"  /* Messungsname */
#define INFLUXDB1_DB_NAME "collectd"  /* Datenbankname für InfluxDB v1 */
#define INFLUXDB1_USER "collectd"     /* Benutzername für InfluxDB v1 */
#define INFLUXDB1_PASSWORD "password" /* Passwort für InfluxDB v1 */
#define INFLUXDB2_TOKEN                                \
  "your_token" /* Authentifizierungs-Token für InfluxDB v2 \
                */
#define INFLUXDB2_ORG \
  "your_organisation"                  /* Organisation für InfluxDB v2 */
#define INFLUXDB2_BUCKET "your_bucket" /* Bucket für InfluxDB v2 */

// Sensoreinstellungen
#define SENSOR_MAX_MEASUREMENTS \
  8 /* Maximale Anzahl Messungen pro Sensortyp */
#define SENSOR_AUTO_DEINITIALIZE \
  false /* Sensoren nach jeder Messung deinitialisieren */

// Einstellungen für Durchflussempfänger
#define SERIAL_RECEIVER_RX_PIN 14       // GPIO14 (D5 beim NodeMCU)
#define SERIAL_RECEIVER_TX_PIN 12       // GPIO12 (D6 beim NodeMCU) - optional
#define SERIAL_RECEIVER_BAUD_RATE 9600  // Serielle Kommunikation (Baudrate)
#define SERIAL_RECEIVER_TIMEOUT 10000   // Timeout in Millisekunden
#define SERIAL_RECEIVER_MAX_MESSAGE_SIZE 512
#define SERIAL_RECEIVER_MEASUREMENT_INTERVAL \
  60                                        // Messintervall in Sekunden
#define SERIAL_RECEIVER_MINIMUM_DELAY 1000  // Minimale Verzögerung zwischen Messungen

// DHT-Einstellungen
#define DHT_PIN 13              // D7
#define DHT_TYPE 22             // 11 für DHT11 oder 22 für DHT22
#define DHT_MINIMUM_DELAY 2500  // in Millisekunden
#define DHT_DEBUG_TIMING false  // Timing-Debugausgaben aktivieren
#define DHT_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL  // Messintervall
#define DHT_TEMPERATURE_NAME "Lufttemperatur"  // Name des Temperatursensors
#define DHT_TEMPERATURE_FIELD_NAME "air_temperature"  // Feldname in InfluxDB
#define DHT_TEMPERATURE_UNIT "°C"  // Einheit des Temperatursensors
#define DHT_TEMPERATURE_MIN                          \
  -40.0f /* Minimaler gültiger Temperaturwert in °C */
#define DHT_TEMPERATURE_MAX                         \
  80.0f /* Maximaler gültiger Temperaturwert in °C */
#define DHT_TEMPERATURE_YELLOW_LOW -10.0f  // Gelb-Untergrenze
#define DHT_TEMPERATURE_GREEN_LOW 0.0f     // Grün-Untergrenze
#define DHT_TEMPERATURE_GREEN_HIGH 30.0f   // Grün-Obergrenze
#define DHT_TEMPERATURE_YELLOW_HIGH 40.0f  // Gelb-Obergrenze
#define DHT_HUMIDITY_NAME "Luftfeuchte"    // Name des Feuchtesensors
#define DHT_HUMIDITY_MIN 1.0f   /* Minimaler gültiger Feuchtewert in % */
#define DHT_HUMIDITY_MAX 100.0f /* Maximaler gültiger Feuchtewert in % */
#define DHT_HUMIDITY_FIELD_NAME "air_humidity"  // Feldname in InfluxDB
#define DHT_HUMIDITY_UNIT "%"                   // Einheit des Feuchtesensors
#define DHT_HUMIDITY_YELLOW_LOW 10.0f           // Gelb-Untergrenze
#define DHT_HUMIDITY_GREEN_LOW 20.0f            // Grün-Untergrenze
#define DHT_HUMIDITY_GREEN_HIGH 80.0f           // Grün-Obergrenze
#define DHT_HUMIDITY_YELLOW_HIGH 90.0f          // Gelb-Obergrenze

// DS18B20-Einstellungen
#define ONE_WIRE_BUS 2  // D4, für DS18B20 Temperatursensor
#define DS18B20_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL
#define DS18B20_MINIMUM_DELAY \
  750                      // minimale Zeit zwischen Messungen in Millisekunden
#define DS18B20_MIN -55.0f /* Minimaler gültiger Temperaturwert in °C */
#define DS18B20_MAX 125.0f /* Maximaler gültiger Temperaturwert in °C */
#define DS18B20_YELLOW_LOW -10.0f        // Gelb-Untergrenze
#define DS18B20_GREEN_LOW 0.0f           // Grün-Untergrenze
#define DS18B20_GREEN_HIGH 90.0f         // Grün-Obergrenze
#define DS18B20_YELLOW_HIGH 100.0f       // Gelb-Obergrenze
#define DS18B20_SENSOR_COUNT 8           // Anzahl der Sensoren
#define DS18B20_UNIT "°C"                // Einheit des Temperatursensors
#define DS18B20_1_NAME "Sensor 1"        // Name des Sensors
#define DS18B20_1_FIELD_NAME "sensor_1"  // Feldname in InfluxDB
#define DS18B20_1_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_1_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_1_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_1_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_1_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_1_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_1_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_1_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_2_NAME "Sensor 2"                  // Name des Sensors
#define DS18B20_2_FIELD_NAME "sensor_2"            // Feldname in InfluxDB
#define DS18B20_2_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_2_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_2_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_2_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_2_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_2_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_2_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_2_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_3_NAME "Sensor 3"                  // Name des Sensors
#define DS18B20_3_FIELD_NAME "sensor_3"            // Feldname in InfluxDB
#define DS18B20_3_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_3_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_3_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_3_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_3_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_3_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_3_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_3_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_4_NAME "Sensor 4"                  // Name des Sensors
#define DS18B20_4_FIELD_NAME "sensor_4"            // Feldname in InfluxDB
#define DS18B20_4_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_4_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_4_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_4_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_4_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_4_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_4_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_4_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_5_NAME "Sensor 5"                  // Name des Sensors
#define DS18B20_5_FIELD_NAME "sensor_5"            // Feldname in InfluxDB
#define DS18B20_5_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_5_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_5_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_5_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_5_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_5_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_5_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_5_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_6_NAME "Sensor 6"                  // Name des Sensors
#define DS18B20_6_FIELD_NAME "sensor_6"            // Feldname in InfluxDB
#define DS18B20_6_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_6_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_6_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_6_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_6_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_6_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_6_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_6_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_7_NAME "Sensor 7"                  // Name des Sensors
#define DS18B20_7_FIELD_NAME "sensor_7"            // Feldname in InfluxDB
#define DS18B20_7_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                     // Messintervall
#define DS18B20_7_UNIT DS18B20_UNIT                // Einheit des Sensors
#define DS18B20_7_MIN DS18B20_MIN                  // minimaler gültiger Messwert
#define DS18B20_7_MAX DS18B20_MAX                  // maximaler gültiger Messwert
#define DS18B20_7_YELLOW_LOW DS18B20_YELLOW_LOW    // Gelb-Untergrenze
#define DS18B20_7_GREEN_LOW DS18B20_GREEN_LOW      // Grün-Untergrenze
#define DS18B20_7_GREEN_HIGH DS18B20_GREEN_HIGH    // Grün-Obergrenze
#define DS18B20_7_YELLOW_HIGH DS18B20_YELLOW_HIGH  // Gelb-Obergrenze
#define DS18B20_8_NAME "Sensor 8"                  // Name des Sensors
#define DS18B20_8_FIELD_NAME "sensor_8"            // Feldname in InfluxDB
#define DS18B20_8_MEASUREMENT_INTERVAL \
  DS18B20_MEASUREMENT_INTERVAL                   // Messintervall
#define DS18B20_8_UNIT DS18B20_UNIT              // Einheit des Sensors
#define DS18B20_8_MIN DS18B20_MIN                // minimaler gültiger Messwert
#define DS18B20_8_MAX DS18B20_MAX                // maximaler gültiger Messwert
#define DS18B20_8_YELLOW_LOW DS18B20_YELLOW_LOW  // Gelb-Untergrenze
#define DS18B20_8_GREEN_LOW DS18B20_GREEN_LOW    // Grün-Untergrenze
#define DS18B20_8_GREEN_HIGH DS18B20_GREEN_HIGH  // Grün-Obergrenze
#define DS18B20_8_YELLOW_HIGH DS18B20_YELLOW_HIGH

// SDS011 particle measurement settings
#define SDS011_MEASUREMENT_INTERVAL 600 /* Measurement interval in seconds */
#define SDS011_MINIMUM_DELAY \
  MEASUREMENT_MINIMUM_DELAY        // minimum delay between measurements in
                                   // milliseconds
#define SDS011_GREEN_HIGH 200.0f   // one sided boundary
#define SDS011_YELLOW_HIGH 300.0f  // yellow high threshold
#define SDS011_WARMUP_TIME \
  35 * 1000  // milliseconds of warumup time for SDS011 sensor
#define SDS011_PM10_NAME "10um Partikel"         // name of the sensor
#define SDS011_PM10_FIELD_NAME "particles_10um"  // field name in influxdb
#define SDS011_PM10_UNIT "ppm"                   // unit of the sensor
#define SDS011_PM10_MIN 0.0f   /* Minimum valid PM10 reading in μg/m³ */
#define SDS011_PM10_MAX 999.9f /* Maximum valid PM10 reading in μg/m³ */
#define SDS011_PM10_GREEN_HIGH SDS011_GREEN_HIGH    // green high threshold
#define SDS011_PM10_YELLOW_HIGH SDS011_YELLOW_HIGH  // yellow high threshold
#define SDS011_PM25_NAME "25um Partikel"            // name of the sensor
#define SDS011_PM25_FIELD_NAME "particles_2.5um"    // field name in influxdb
#define SDS011_PM25_UNIT "ppm"                      // unit of the sensor
#define SDS011_PM25_MIN 0.0f   /* Minimum valid PM2.5 reading in μg/m³ */
#define SDS011_PM25_MAX 999.9f /* Maximum valid PM2.5 reading in μg/m³ */
#define SDS011_PM25_GREEN_HIGH SDS011_GREEN_HIGH    // green high threshold
#define SDS011_PM25_YELLOW_HIGH SDS011_YELLOW_HIGH  // yellow high threshold
#define SDS011_RX_PIN 5                             /* RX pin for SDS011 */
#define SDS011_TX_PIN 4                             /* TX pin for SDS011 */

// MHZ19 CO2 Sensor settings
#define MHZ19_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL  // measurement interval
#define MHZ19_MINIMUM_DELAY \
  MEASUREMENT_MINIMUM_DELAY    // minimum delay between measurements in
                               // milliseconds
#define MHZ19_MIN 1.0f         /* Minimum valid CO2 reading in ppm */
#define MHZ19_MAX 5000.0f      /* Maximum valid CO2 reading in ppm */
#define MHZ19_YELLOW_LOW 300   /* Lower yellow threshold */
#define MHZ19_GREEN_LOW 400    /* Lower green threshold */
#define MHZ19_GREEN_HIGH 1000  /* Upper green threshold */
#define MHZ19_YELLOW_HIGH 2000 /* Upper yellow threshold */
#define MHZ19_PWM_PIN 4        /* PWM pin for MHZ19 */
#define MHZ19_RX_PIN 22        // will implement later
#define MHZ19_TX_PIN 21        // will implement later

#define ANALOG_SENSOR_COUNT 8  // more than one needs Multiplexer
#define ANALOG_MEASUREMENT_INTERVAL \
  MEASUREMENT_INTERVAL  // measurement interval
#define ANALOG_MINIMUM_DELAY \
  100                  // minimum delay between measurements in milliseconds
#define ANALOG_PIN A0  // pin of the analog sensor
#define ANALOG_CALIBRATION_MODE false   // activate autocalibration
#define ANALOG_MIN 50                   // minimum valid reading
#define ANALOG_MAX 900                  // maximum valid reading
#define ANALOG_YELLOW_LOW 10.0f         // yellow low threshold
#define ANALOG_GREEN_LOW 20.0f          // green low threshold
#define ANALOG_GREEN_HIGH 80.0f         // green high threshold
#define ANALOG_YELLOW_HIGH 90.0f        // yellow high threshold
#define ANALOG_1_NAME "Analog 1"        // name of the sensor
#define ANALOG_1_FIELD_NAME "analog_1"  // field name in influxdb
#define ANALOG_1_MIN ANALOG_MIN         // minimum valid reading
#define ANALOG_1_MAX ANALOG_MAX         // maximum valid reading
#define ANALOG_1_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_1_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_1_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_1_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_1_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_1_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_2_NAME "Analog 2"                 // name of the sensor
#define ANALOG_2_FIELD_NAME "analog_2"           // field name in influxdb
#define ANALOG_2_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_2_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_2_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_2_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_2_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_2_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_2_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_2_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_3_NAME "Analog 3"                 // name of the sensor
#define ANALOG_3_FIELD_NAME "analog_3"           // field name in influxdb
#define ANALOG_3_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_3_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_3_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_3_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_3_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_3_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_3_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_3_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_4_NAME "Analog 4"                 // name of the sensor
#define ANALOG_4_FIELD_NAME "analog_4"           // field name in influxdb
#define ANALOG_4_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_4_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_4_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_4_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_4_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_4_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_4_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_4_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_5_NAME "Analog 5"                 // name of the sensor
#define ANALOG_5_FIELD_NAME "analog_5"           // field name in influxdb
#define ANALOG_5_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_5_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_5_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_5_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_5_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_5_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_5_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_5_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_6_NAME "Analog 6"                 // name of the sensor
#define ANALOG_6_FIELD_NAME "analog_6"           // field name in influxdb
#define ANALOG_6_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_6_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_6_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_6_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_6_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_6_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_6_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_6_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_7_NAME "Analog 7"                 // name of the sensor
#define ANALOG_7_FIELD_NAME "analog_7"           // field name in influxdb
#define ANALOG_7_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_7_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_7_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_7_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_7_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_7_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_7_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_7_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold
#define ANALOG_8_NAME "Analog 8"                 // name of the sensor
#define ANALOG_8_FIELD_NAME "analog_8"           // field name in influxdb
#define ANALOG_8_MIN ANALOG_MIN                  // minimum valid reading
#define ANALOG_8_MAX ANALOG_MAX                  // maximum valid reading
#define ANALOG_8_CALIBRATION_MODE \
  ANALOG_CALIBRATION_MODE  // activate autocalibration
#define ANALOG_8_INVERTED \
  false  // invert scale (true = inverted, false = normal)
#define ANALOG_8_YELLOW_LOW ANALOG_YELLOW_LOW    // yellow low threshold
#define ANALOG_8_GREEN_LOW ANALOG_GREEN_LOW      // green low threshold
#define ANALOG_8_GREEN_HIGH ANALOG_GREEN_HIGH    // green high threshold
#define ANALOG_8_YELLOW_HIGH ANALOG_YELLOW_HIGH  // yellow high threshold

// Multiplexer settings
#define MULTIPLEXER_PIN_A 15 /* Multiplexer control pin A */
#define MULTIPLEXER_PIN_B 2  /* Multiplexer control pin B */
#define MULTIPLEXER_PIN_C 16 /* Multiplexer control pin C */

// LED Traffic Light settings
#define LED_GREEN_PIN 14  /* Green LED pin */
#define LED_YELLOW_PIN 12 /* Yellow LED pin */
#define LED_RED_PIN 13    /* Red LED pin */

// Display settings (SSD1306)
#define DISPLAY_WIDTH 128      /* Display width in pixels */
#define DISPLAY_HEIGHT 64      /* Display height in pixels */
#define DISPLAY_DEFAULT_TIME 5 /* Default display time in seconds */
#define DISPLAY_RX_PIN 5       /* Display RX pin */
#define DISPLAY_TX_PIN 8       /* Display TX pin */
#define DISPLAY_RESET -1       /* Display reset pin */
#define DISPLAY_ADDRESS 0x3C   /* Display I2C address */

#endif  // CONFIG_H
