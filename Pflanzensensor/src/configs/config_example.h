/**
 * @file config_pflanzensensor.h
 * @brief Konfigurationsdatei für das ESP8266-basierte Sensorsystem
 */

// Geräteeinstellungen
#define DEVICE_NAME "Fabmobil Pflanzensensor"
#define LOG_LEVEL "Info" // Mögliche Werte: INFO, DEBUG, ERROR, WARNING

// Feature-Flags
#define USE_DHT true               // DHT11 oder DHT22 Temperatur- und Feuchtesensoren
#define USE_ANALOG true            // Aktiviert analoge Sensorfunktionalität
#define USE_MULTIPLEXER true       // Viele analoge Sensoren mit Multiplexer verwenden
#define USE_DISPLAY true           // Display verwenden
#define USE_LED_TRAFFIC_LIGHT true // LED-Ampel verwenden
#define USE_WEBSERVER true         // Webserver-Funktionalität verwenden, nur mit USE_WIFI
#define USE_WEBSOCKET true         // Websocket-Modul für Logs verwenden
#define USE_WIFI true              // WLAN des ESP verwenden

// Debug-Flags
#define DEBUG_RAM true                /* RAM-Debugmeldungen aktivieren */
#define DEBUG_MEASUREMENT_CYCLE false /* Debugmeldungen für Messzyklen aktivieren */
#define DEBUG_SENSOR false            /* Debugmeldungen für Sensoren aktivieren */
#define DEBUG_DISPLAY false           /* Debugmeldungen für Display aktivieren */
#define DEBUG_WEBSOCKET false         /* Debugmeldungen für WebSocket aktivieren */

// Messeinstellungen
#define MEASUREMENT_INTERVAL 60 // in Sekunden
#define MEASUREMENT_DEINITIALIZE_SENSORS false
#define MEASUREMENT_MINIMUM_DELAY                                                                  \
  500 // Mindestverzögerung zwischen Messungen in Millisekunden (erhöht von 100ms)
#define MEASUREMENT_AVERAGE_COUNT 3 // Anzahl aufeinanderfolgender Messungen für Mittelwertbildung
#define MEASUREMENT_ERROR_COUNT 5 // Anzahl aufeinanderfolgender Fehlmessungen vor Reinit und Fehler

// Netzwerkeinstellungen
#define WIFI_SSID_1 ""
#define WIFI_PASSWORD_1 ""
#define WIFI_SSID_2 ""
#define WIFI_PASSWORD_2 ""
#define WIFI_SSID_3 ""
#define WIFI_PASSWORD_3 ""
#define HOSTNAME DEVICE_NAME
#define USE_STATIC_IP 0 // 1 für statische IP, 0 für DHCP
// Diese Einstellungen sind nur relevant, wenn USE_STATIC_IP auf 1 gesetzt ist:
#define STATIC_IP 172, 17, 1, 44  // Gewünschte statische IP-Adresse
#define GATEWAY 172, 17, 1, 1     // IP-Adresse des Routers
#define SUBNET 255, 255, 0, 0     // Subnetzmaske
#define PRIMARY_DNS 172, 17, 1, 1 // Primärer DNS
#define SECONDARY_DNS 8, 8, 4, 4  // Sekundärer DNS

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
#define LED_GREEN_PIN 14
#define LED_YELLOW_PIN 12
#define LED_RED_PIN 13

// SSD1306 Display (später implementieren)
#define DISPLAY_WIDTH 128 // OLED-Display-Breite in Pixel
#define DISPLAY_HEIGHT 64
#define DISPLAY_DEFAULT_TIME 5 // in Sekunden
#define DISPLAY_RX_PIN 5
#define DISPLAY_TX_PIN 8
#define DISPLAY_RESET -1
#define DISPLAY_ADDRESS 0x3C

// alles hier drunter fliegt irgendwann raus ..
#define FILE_LOGGING_ENABLED false
#define MAX_LOG_FILE_SIZE 50000 // Maximale Logdateigröße in Bytes
#define USE_MAIL                                                                                   \
  false // E-Mail-Benachrichtigungen verwenden. Wir haben nicht genügend RAM für TLS :/
#define DHT_TEMPERATURE_FIELD_NAME "lufttemperatur" // für InfluxDB
#define DHT_HUMIDITY_FIELD_NAME "luftfeuchte"       // für InfluxDB
