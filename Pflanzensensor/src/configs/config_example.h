/**
 * @file config_example.h
 * @brief Beispielkonfiguration für das ESP8266-basierte Sensorsystem
 *
 * Diese Datei als configs/config_pflanzensensor.h kopieren und anpassen -
 * die eigene Konfiguration ist bewusst nicht versioniert, damit WLAN-Zugang
 * und Passwörter nicht im Repository landen.
 *
 * Sie muss übersetzbar bleiben: die CI baut die Firmware gegen genau diese
 * Datei. Kommt im Quelltext ein neues #define hinzu, gehört es auch hierher.
 */

// Geräteeinstellungen
#define DEVICE_NAME "Fabmobil Pflanzensensor"
#define LOG_LEVEL "Info" // Mögliche Werte: INFO, DEBUG, ERROR, WARNING

// Feature-Flags
#define USE_DHT 1                // DHT11 oder DHT22 Temperatur- und Feuchtesensoren
#define USE_ANALOG 1             // Aktiviert analoge Sensorfunktionalität
#define USE_MULTIPLEXER 1        // Viele analoge Sensoren mit Multiplexer verwenden
#define USE_DISPLAY 1            // Display verwenden
#define USE_LED_TRAFFIC_LIGHT 1  // LED-Ampel verwenden
#define USE_WEBSERVER 1          // Webserver-Funktionalität verwenden, nur mit USE_WIFI
#define USE_WEBSOCKET 1          // Websocket-Modul für Logs verwenden
#define USE_WIFI 1               // WLAN des ESP verwenden
#define USE_PROMETHEUS_METRICS 1 // Prometheus Metrics Exporter verwenden
#define USE_DS18B20 0            // DS18B20 Temperature Sensor (up to 8 on one pin)
#define USE_SDS011 0             // SDS011 particle measurement sensor (needs wakeup)
#define USE_MHZ19 0              // MHZ19 CO2 sensor (needs wakeup)
#define USE_HX711 0              // HX711 weight measurement sensor
#define USE_BMP280 0             // BMP280 temperature and pressure sensor
#define USE_SERIAL_RECEIVER 0    // Serial receiver for various sensors

// Debug-Flags
#define DEBUG_RAM 0               /* RAM-Debugmeldungen aktivieren */
#define DEBUG_MEASUREMENT_CYCLE 0 /* Debugmeldungen für Messzyklen aktivieren */
#define DEBUG_SENSOR 0            /* Debugmeldungen für Sensoren aktivieren */
#define DEBUG_DISPLAY 0           /* Debugmeldungen für Display aktivieren */
#define DEBUG_WEBSOCKET 0         /* Debugmeldungen für WebSocket aktivieren */
#define DEBUG_METRICS 0           /* Debugmeldungen für Prometheus Metrics aktivieren */

// Messeinstellungen
#define MEASUREMENT_INTERVAL 60 // in Sekunden
#define MEASUREMENT_DEINITIALIZE_SENSORS false
// Mindestverzoegerung zwischen Messungen in Millisekunden
#define MEASUREMENT_MINIMUM_DELAY 500
#define MEASUREMENT_AVERAGE_COUNT 3 // Anzahl aufeinanderfolgender Messungen für Mittelwertbildung
#define MEASUREMENT_ERROR_COUNT 5 // Anzahl aufeinanderfolgender Fehlmessungen vor Reinit und Fehler

// Netzwerkeinstellungen
#define HOST_IP 192, 168, 1, 100 // read from deploy script as OTA update target
#define WIFI_SSID_1 ""
#define WIFI_PASSWORD_1 ""
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
#define WIFI_SSID_3 ""
#define WIFI_PASSWORD_3 ""
#define HOSTNAME DEVICE_NAME
#define USE_STATIC_IP 0 // 1 für statische IP, 0 für DHCP
// Diese Einstellungen sind nur relevant, wenn USE_STATIC_IP auf 1 gesetzt ist:
#define STATIC_IP 192, 168, 1, 100 // Gewünschte statische IP-Adresse
#define GATEWAY 192, 168, 1, 1     // IP-Adresse des Routers
#define SUBNET 255, 255, 0, 0      // Subnetzmaske
#define PRIMARY_DNS 192, 168, 1, 1 // Primärer DNS
#define SECONDARY_DNS 8, 8, 4, 4   // Sekundärer DNS

// Webserver-Einstellungen
#define LOG_ENTRIES_TO_DISPLAY 20
#define ADMIN_PASSWORD "Fabmobil" // Initiales Admin-Passwort für Webinterface

// DHT-Sensor-Einstellungen
#define DHT_PIN 0 // D3
#define DHT_TYPE 11
#define DHT_MINIMUM_DELAY 1000 // in Millisekunden
#define DHT_DEBUG_TIMING false // true für detaillierte Timing-Informationen im Log
#define DHT_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL

// DHT Temperatur (Messung 1)
#define DHT_TEMPERATURE_NAME "Lufttemperatur"
#define DHT_TEMPERATURE_UNIT "°C"
#define DHT_TEMPERATURE_YELLOW_LOW 10.0f
#define DHT_TEMPERATURE_GREEN_LOW 15.0f
#define DHT_TEMPERATURE_GREEN_HIGH 25.0f
#define DHT_TEMPERATURE_YELLOW_HIGH 30.0f

// DHT Luftfeuchte (Messung 2)
#define DHT_HUMIDITY_NAME "Luftfeuchte"
#define DHT_HUMIDITY_UNIT "%"
#define DHT_HUMIDITY_YELLOW_LOW 20.0f
#define DHT_HUMIDITY_GREEN_LOW 30.0f
#define DHT_HUMIDITY_GREEN_HIGH 80.0f
#define DHT_HUMIDITY_YELLOW_HIGH 90.0f

// Analogsensor(en)
#define ANALOG_SENSOR_COUNT 2 // Mehr als einer benötigt Multiplexer
#define ANALOG_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL
#define ANALOG_MINIMUM_DELAY MEASUREMENT_MINIMUM_DELAY
#define ANALOG_PIN A0
#define ANALOG_CALIBRATION_MODE false // Autokalibrierung (de)aktivieren
#define ANALOG_UNIT "%"
#define ANALOG_MIN 250
#define ANALOG_MAX 750
#define ANALOG_YELLOW_LOW 10.0f
#define ANALOG_GREEN_LOW 20.0f
#define ANALOG_GREEN_HIGH 80.0f
#define ANALOG_YELLOW_HIGH 90.0f
#define ANALOG_1_NAME "Lichtstärke"
#define ANALOG_1_FIELD_NAME "lichtstaerke"
#define ANALOG_1_UNIT ANALOG_UNIT
#define ANALOG_1_MIN 5
#define ANALOG_1_MAX 1023
#define ANALOG_1_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_1_INVERTED false // Lichtsensor: niedriger Wert = dunkel, hoher Wert = hell
#define ANALOG_1_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_1_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_1_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_1_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_2_NAME "Bodenfeuchte"
#define ANALOG_2_FIELD_NAME "bodenfeuchte"
#define ANALOG_2_UNIT ANALOG_UNIT
#define ANALOG_2_MIN ANALOG_MIN
#define ANALOG_2_MAX ANALOG_MAX
#define ANALOG_2_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_2_INVERTED true // Bodenfeuchte: hoher Wert = trocken, niedriger Wert = nass
#define ANALOG_2_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_2_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_2_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_2_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_3_NAME "Bodenfeuchte 2"
#define ANALOG_3_FIELD_NAME "bodenfeuchte_2"
#define ANALOG_3_UNIT ANALOG_UNIT
#define ANALOG_3_MIN ANALOG_MIN
#define ANALOG_3_MAX ANALOG_MAX
#define ANALOG_3_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_3_INVERTED true // Bodenfeuchte: hoher Wert = trocken, niedriger Wert = nass
#define ANALOG_3_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_3_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_3_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_3_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_4_NAME "Bodenfeuchte 3"
#define ANALOG_4_FIELD_NAME "bodenfeuchte_3"
#define ANALOG_4_UNIT ANALOG_UNIT
#define ANALOG_4_MIN ANALOG_MIN
#define ANALOG_4_MAX ANALOG_MAX
#define ANALOG_4_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_4_INVERTED true
#define ANALOG_4_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_4_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_4_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_4_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_5_NAME "Bodenfeuchte 4"
#define ANALOG_5_FIELD_NAME "bodenfeuchte_4"
#define ANALOG_5_UNIT ANALOG_UNIT
#define ANALOG_5_MIN ANALOG_MIN
#define ANALOG_5_MAX ANALOG_MAX
#define ANALOG_5_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_5_INVERTED true
#define ANALOG_5_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_5_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_5_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_5_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_6_NAME "Bodenfeuchte 5"
#define ANALOG_6_FIELD_NAME "bodenfeuchte_5"
#define ANALOG_6_UNIT ANALOG_UNIT
#define ANALOG_6_MIN ANALOG_MIN
#define ANALOG_6_MAX ANALOG_MAX
#define ANALOG_6_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_6_INVERTED true
#define ANALOG_6_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_6_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_6_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_6_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_7_NAME "Bodenfeuchte 6"
#define ANALOG_7_FIELD_NAME "bodenfeuchte_6"
#define ANALOG_7_UNIT ANALOG_UNIT
#define ANALOG_7_MIN ANALOG_MIN
#define ANALOG_7_MAX ANALOG_MAX
#define ANALOG_7_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_7_INVERTED true
#define ANALOG_7_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_7_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_7_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_7_YELLOW_HIGH ANALOG_YELLOW_HIGH
#define ANALOG_8_NAME "Bodenfeuchte 7"
#define ANALOG_8_FIELD_NAME "bodenfeuchte_7"
#define ANALOG_8_UNIT ANALOG_UNIT
#define ANALOG_8_MIN ANALOG_MIN
#define ANALOG_8_MAX ANALOG_MAX
#define ANALOG_8_CALIBRATION_MODE ANALOG_CALIBRATION_MODE
#define ANALOG_8_INVERTED true
#define ANALOG_8_YELLOW_LOW ANALOG_YELLOW_LOW
#define ANALOG_8_GREEN_LOW ANALOG_GREEN_LOW
#define ANALOG_8_GREEN_HIGH ANALOG_GREEN_HIGH
#define ANALOG_8_YELLOW_HIGH ANALOG_YELLOW_HIGH

// Multiplexer-Einstellungen
#define MULTIPLEXER_PIN_A 15
#define MULTIPLEXER_PIN_B 2
#define MULTIPLEXER_PIN_C 16

// LED-Ampel
#define LED_TRAFFIC_LIGHT_ONLY_RED                                                                 \
  false // Nur rote LED verwenden (z.B. als Statusanzeige), sonst alle drei LEDs für Ampel
#define LED_GREEN_PIN 14
#define LED_YELLOW_PIN 12
#define LED_RED_PIN 13

// SSD1306 Display
#define DISPLAY_WIDTH 128 // OLED-Display-Breite in Pixel
#define DISPLAY_HEIGHT 64
#define DISPLAY_DEFAULT_TIME 5 // in Sekunden
#define DISPLAY_RX_PIN 5
#define DISPLAY_TX_PIN 8
#define DISPLAY_RESET -1
#define DISPLAY_ADDRESS 0x3C

// alles hier drunter fliegt irgendwann raus ..
#define FILE_LOGGING_ENABLED false

// ===== Mailversand =====
// Vorbelegt mit dem Funktionskonto des Fabmobil-Pflanzensensors: so muss im
// Webinterface nur noch das Passwort und die Empfängeradresse eingetragen
// werden. Das Passwort steht bewusst NICHT hier - der ESP8266 kennt weder
// Flash-Verschlüsselung noch Secure Boot, und die Firmware wird veröffentlicht;
// alles, was hier steht, ist damit öffentlich lesbar.
#define MAIL_ENABLED false
#define MAIL_SMTP_HOST "smtp.datenkollektiv.net"
#define MAIL_SMTP_PORT 465
#define MAIL_SMTP_USER "pflanzensensor@fabmobil.org"
#define MAIL_SMTP_PASSWORD ""
#define MAIL_FROM "pflanzensensor@fabmobil.org"
#define MAIL_TO ""
#define MAIL_WARN_INTERVAL_HOURS 4
#define MAIL_ALIVE_ENABLED false
#define MAIL_ALIVE_INTERVAL_HOURS 24
#define MAIL_BOOT_ENABLED false

#define MAX_LOG_FILE_SIZE 50000 // Maximale Logdateigröße in Bytes
// E-Mail-Benachrichtigungen. Aus: fuer TLS reicht der RAM nicht.
#define USE_MAIL false
#define DHT_TEMPERATURE_FIELD_NAME "lufttemperatur" // für InfluxDB
#define DHT_HUMIDITY_FIELD_NAME "luftfeuchte"       // für InfluxDB

// DS18B20 Temperature Sensor (OneWire)
#if USE_DS18B20
#define ONE_WIRE_BUS 4         // D2 - OneWire bus pin (GPIO4)
#define DS18B20_SENSOR_COUNT 1 // Anzahl DS18B20 Sensoren am Bus (max. 8)
#define DS18B20_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL
#define DS18B20_MINIMUM_DELAY 750 // Konversionszeit für 12-Bit Auflösung
#define DS18B20_WAKEUP_TIME 0     // Kein Aufwachen nötig
#define DS18B20_1_NAME "DS18B20_1"
#define DS18B20_1_FIELD_NAME "ds18b20_1"
#define DS18B20_1_YELLOW_LOW -10.0f
#define DS18B20_1_GREEN_LOW 0.0f
#define DS18B20_1_GREEN_HIGH 40.0f
#define DS18B20_1_YELLOW_HIGH 60.0f
#define DS18B20_1_MEASUREMENT_INTERVAL DS18B20_MEASUREMENT_INTERVAL
#endif

// SDS011 Feinstaub-Sensor (Serial)
#if USE_SDS011
#define SDS011_RX_PIN 12                // D6 (GPIO12)
#define SDS011_TX_PIN 13                // D7 (GPIO13)
#define SDS011_MEASUREMENT_INTERVAL 600 // 10 Minuten zwischen Messungen
#define SDS011_WAKEUP_TIME 30000        // 30 Sekunden Aufwärmen vor Messung
#define SDS011_PM25_NAME "PM2.5"
#define SDS011_PM25_UNIT "µg/m³"
#define SDS011_PM10_NAME "PM10"
#define SDS011_PM10_UNIT "µg/m³"
#define SDS011_PIN_RX SDS011_RX_PIN
#define SDS011_PIN_TX SDS011_TX_PIN
#define SDS011_WARMUP_TIME SDS011_WAKEUP_TIME
#define SDS011_MINIMUM_DELAY 100
#define SDS011_PM10_FIELD_NAME "pm10"
#define SDS011_PM25_FIELD_NAME "pm25"
#define SDS011_PM10_GREEN_HIGH 25.0f
#define SDS011_PM10_YELLOW_HIGH 50.0f
#define SDS011_PM25_GREEN_HIGH 15.0f
#define SDS011_PM25_YELLOW_HIGH 35.0f
#endif

// MHZ19 CO2-Sensor (Serial)
#if USE_MHZ19
#define MHZ19_RX_PIN 14 // D5 (GPIO14) - SoftwareSerial RX
#define MHZ19_TX_PIN 12 // D6 (GPIO12) - SoftwareSerial TX
#define MHZ19_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL
#define MHZ19_WAKEUP_TIME 180000 // 3 Minuten Aufwärmen für Basislinie
#define MHZ19_CO2_NAME "CO2"
#define MHZ19_CO2_UNIT "ppm"
#define MHZ19_TEMP_NAME "MHZ19_Temperatur"
#define MHZ19_TEMP_UNIT "°C"
#define MHZ19_PIN_RX MHZ19_RX_PIN
#define MHZ19_PWM_PIN MHZ19_PIN_RX
#define MHZ19_WARMUP_TIME MHZ19_WAKEUP_TIME
#define MHZ19_MINIMUM_DELAY 2000
#define MHZ19_NAME "CO2"
#define MHZ19_FIELD_NAME "co2"
#define MHZ19_UNIT "ppm"
#define MHZ19_MIN 0.0f
#define MHZ19_MAX 5000.0f
#define MHZ19_YELLOW_LOW 400.0f
#define MHZ19_GREEN_LOW 600.0f
#define MHZ19_GREEN_HIGH 1200.0f
#define MHZ19_YELLOW_HIGH 2000.0f
#endif

// HX711 Gewichtssensor
#if USE_HX711
#define HX711_SCK_PIN 5  // D1 (GPIO5)
#define HX711_DOUT_PIN 4 // D2 (GPIO4)
#define HX711_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL
#define HX711_MINIMUM_DELAY 50 // Konversionszeit
#define HX711_WAKEUP_TIME 0    // Kein Aufwachen nötig
#define HX711_SCALE -7050.0f   // Kalibrierungswert
#define HX711_OFFSET 0         // Tara-Offset
#define HX711_WEIGHT_NAME "Gewicht"
#define HX711_WEIGHT_UNIT "g"
#define HX711_NAME HX711_WEIGHT_NAME
#define HX711_FIELD_NAME "gewicht"
#define HX711_UNIT HX711_WEIGHT_UNIT
#define HX711_YELLOW_LOW 0.0f
#define HX711_GREEN_LOW 10.0f
#define HX711_GREEN_HIGH 5000.0f
#define HX711_YELLOW_HIGH 10000.0f
#endif

// BMP280 Temperatur- und Luftdrucksensor (I2C via SPI-Pins)
#if USE_BMP280
#define BMP280_SCK_PIN 5 // D1 (GPIO5) - I2C SCL
#define BMP280_SDI_PIN 4 // D2 (GPIO4) - I2C SDA
#define BMP280_MEASUREMENT_INTERVAL MEASUREMENT_INTERVAL
#define BMP280_MINIMUM_DELAY 100
#define BMP280_WAKEUP_TIME 0 // Kein Aufwachen nötig
#define BMP280_PRESSURE_NAME "Luftdruck"
#define BMP280_PRESSURE_UNIT "hPa"
#define BMP280_TEMP_NAME "BMP_Temperatur"
#define BMP280_TEMP_UNIT "°C"
#define BMP280_ALTITUDE 50 // Höhe in Metern für Meereshöhendruck
#define BMP280_SDA_PIN BMP280_SDI_PIN
#define BMP280_SCL_PIN BMP280_SCK_PIN
#define BMP280_TEMPERATURE_NAME BMP280_TEMP_NAME
#define BMP280_TEMPERATURE_FIELD_NAME "bmp280_temperatur"
#define BMP280_PRESSURE_FIELD_NAME "luftdruck"
#define BMP280_TEMPERATURE_YELLOW_LOW -10.0f
#define BMP280_TEMPERATURE_GREEN_LOW 10.0f
#define BMP280_TEMPERATURE_GREEN_HIGH 35.0f
#define BMP280_TEMPERATURE_YELLOW_HIGH 50.0f
#define BMP280_PRESSURE_YELLOW_LOW 950.0f
#define BMP280_PRESSURE_GREEN_LOW 980.0f
#define BMP280_PRESSURE_GREEN_HIGH 1030.0f
#define BMP280_PRESSURE_YELLOW_HIGH 1060.0f
#endif

// Serial Receiver (für benutzerdefinierte Sensoren)
#if USE_SERIAL_RECEIVER
#define SERIAL_RECEIVER_RX_PIN 14 // D5 (GPIO14)
#define SERIAL_RECEIVER_TX_PIN 12 // D6 (GPIO12) - optional
#define SERIAL_RECEIVER_BAUD 9600
#define SERIAL_RECEIVER_MEASUREMENT_INTERVAL 60
#define SERIAL_RECEIVER_PIN_RX SERIAL_RECEIVER_RX_PIN
#define SERIAL_RECEIVER_PIN_TX SERIAL_RECEIVER_TX_PIN
#define SERIAL_RECEIVER_TIMEOUT 4000
#endif
