/**
 * @file Pflanzensensor.ino
 * @brief Hauptprogramm des Fabmobil Pflanzensensors
 * @author Tommy, Claude
 * @date 2024-10-23
 *
 * Diese Datei enthält die Hauptfunktionen setup() und loop() sowie
 * zentrale Funktionen für den Betrieb des Pflanzensensors.
 */

#include "einstellungen.h"
#include "passwoerter.h"
#include "logger.h"
#include "sensoren.h"
#include "display.h"
#include "wifi.h"
#include "webhook.h"
#include "ledampel.h"

// Globale Instanzen
SensorManager sensorManager;
mutex_t mutex;

// Intervall-Timer
unsigned long millisVorherDisplay = 0;
unsigned long millisVorherWebhook = 0;
unsigned long millisVorherWebhookPing = 0;

void setup() {
    // Serielle Verbindung initialisieren
    Serial.begin(baudrateSeriell);
    logger.SetzteLogLevel(LogLevel::INFO);
    logger.NTPInitialisieren();
    delay(100);

    // Display initialisieren
    DisplaySetup();
    DisplayDreiWoerter(F("Start.."), F(" bitte"), F(" warten!"));

    // Mutex erstellen
    CreateMutex(&mutex);
    logger.debug(F("Start von setup()"));
    logger.info(F("Fabmobil Pflanzensensor, v") + String(pflanzensensorVersion));

    // LED Ampel initialisieren
    pinMode(ampelPinGruen, OUTPUT);
    pinMode(ampelPinGelb, OUTPUT);
    pinMode(ampelPinRot, OUTPUT);

    // Funktionstest der LEDs
    LedampelBlinken("gruen", 1, 300);
    LedampelBlinken("gelb", 1, 300);
    LedampelBlinken("rot", 1, 300);

    // Dateisystem initialisieren
    if (!LittleFS.begin()) {
        logger.error(F("Fehler: LittleFS konnte nicht initialisiert werden!"));
        DisplayDreiWoerter(F("Start.."), F(" LittleFS"), F("  Fehler!"));
        return;
    }

    // Einstellungen laden
    variablen.begin("pflanzensensor", false);
    if (VariablenDa()) {
        DisplayDreiWoerter(F("Start.."), F(" Variablen"), F("  laden"));
        variablen.end();
        VariablenLaden();
    } else {
        DisplayDreiWoerter(F("Start.."), F(" Variablen"), F("  speichern"));
        variablen.end();
        VariablenSpeichern();
    }

    // Neustarts zählen
    variablen.begin("pflanzensensor", false);
    neustarts = variablen.getInt("neustarts", 0);
    neustarts++;
    variablen.putInt("neustarts", neustarts);
    variablen.end();
    logger.info(F("Neustarts: ") + String(neustarts));

    // WiFi Setup
    String ip = WifiSetup(wifiHostname);
    if (ip == "keine WLAN Verbindung.") {
        logger.warning(F("Keine WLAN-Verbindung möglich. Wechsel in den Accesspoint-Modus."));
        DisplayDreiWoerter(F("Kein WLAN"), F("Starte"), F("Accesspoint"));
        wifiAp = true;
        ip = WifiSetup(wifiHostname);
    }

    // Webhook Setup wenn WiFi verfügbar
    if (!wifiAp && webhookAn) {
        DisplayDreiWoerter(F("Start.."), F(" Webhook-"), F("  modul"));
        WebhookSetup();
    }

    // Sensoren initialisieren
    sensorManager.initialisiere();

    DisplayDreiWoerter(F("Start.."), F(" abge-"), F(" schlossen"));
    logger.info(F("Start abgeschlossen!"));
}

void loop() {
    millisAktuell = millis();

    HandleRestart();
        if (millisAktuell - millisVorherAnalog >= intervallMessung - 2000) {
            logger.NTPUpdaten();
            logger.info(F("IP Adresse: ") + String(ip));

            // HEAP Informationen sammeln
            uint32_t freierHeap = ESP.getFreeHeap();
            uint32_t groessterHeapBlock = ESP.getMaxFreeBlockSize();
            uint8_t heapFragmentierung = 100 - ((groessterHeapBlock * 100) / freierHeap);

            // Flash Informationen sammeln
            FSInfo flashInfo;
            LittleFS.info(flashInfo);
            uint32_t freierFlash = flashInfo.totalBytes - flashInfo.usedBytes;

            // Informationen loggen
            logger.debug(F("HEAP-Speicher:"));
            logger.debug(F("- Frei: ") + String(freierHeap) + F(" Bytes"));
            logger.debug(F("- Größter freier Block: ") + String(groessterHeapBlock) + F(" Bytes"));
            logger.debug(F("- Fragmentierung: ") + String(heapFragmentierung) + F("%"));
            logger.debug(F("Flash-Speicher:"));
            logger.debug(F("- Gesamt: ") + String(flashInfo.totalBytes) + F(" Bytes"));
            logger.debug(F("- Benutzt: ") + String(flashInfo.usedBytes) + F(" Bytes"));
            logger.debug(F("- Frei: ") + String(freierFlash) + F(" Bytes"));
        }

        // Sensoren messen
        if (GetMutex(&mutex)) {
            sensorManager.messungenDurchfuehren();
            ReleaseMutex(&mutex);
        }

        // Display aktualisieren
        if (displayAn && millisAktuell - millisVorherDisplay >= intervallDisplay) {
            millisVorherDisplay = millisAktuell;
            DisplayAnzeigen();
            NaechsteSeite();
        }

        // LED Ampel aktualisieren
        if (ampelAn && ampelModus == 0) {
          auto* sensor = sensorManager.holeSensor("Bodenfeuchte");
          if (sensor && sensor->holeSensorTyp() == SensorTyp::ANALOG) {
              LedampelAnzeigen(sensor->holeFarbe(), -1);
          }
        }

        // WiFi und Webserver verwalten
        if (GetMutex(&mutex)) {
            if (!wifiAp) {
                if (wifiMulti.run(wifiTimeout) == WL_CONNECTED) {
                    ip = WiFi.localIP().toString();
                    aktuelleSsid = WiFi.SSID();
                    wifiVerbindungsVersuche = 0;
                } else {
                    wifiVerbindungsVersuche++;
                    if (wifiVerbindungsVersuche >= 10) {
                        logger.warning(F("Fehler: WLAN Verbindung verloren! Wechsle in den Accesspoint-Modus."));
                        DisplayDreiWoerter(F("WLAN"), F("Verbindung"), F("verloren!"));
                        wifiAp = true;
                        String ip = WifiSetup(wifiHostname);
                        aktuelleSsid = wifiApSsid;
                        wifiVerbindungsVersuche = 0;
                    } else {
                        logger.info(F("WLAN-Verbindungsversuch fehlgeschlagen. Versuch ") +
                                  String(wifiVerbindungsVersuche) + F(" von 10"));
                    }
                }
            }
            Webserver.handleClient();
            ReleaseMutex(&mutex);
        }

        // Webhook verarbeiten
        if (webhookAn && !wifiAp) {
            unsigned long aktuelleZeit = millis();
            bool aktuellerAlarmStatus = false;

            // Alarmstatus der aktiven Sensoren prüfen
            for (auto* sensor : sensorManager.holeSensoren()) {
                if (sensor->istAktiv() && sensor->istWebhookAlarmAktiv() &&
                    sensor->holeFarbe() == "rot") {
                    aktuellerAlarmStatus = true;
                    break;
                }
            }

            String neuerStatus = aktuellerAlarmStatus ? "Alarm" : "OK";

            // Prüfen ob reguläre Übertragung oder Ping fällig ist
            bool sendeAlarm = (aktuelleZeit - millisVorherWebhook >=
                             (unsigned long)(webhookFrequenz) * 1000UL * 60UL * 60UL);
            bool sendePing = (aktuelleZeit - millisVorherWebhookPing >=
                            (unsigned long)(webhookPingFrequenz) * 1000UL * 60UL * 60UL);

            if (sendePing) {
                WebhookErfasseSensordaten("ping");
                millisVorherWebhookPing = aktuelleZeit;
                letzterWebhookStatus = neuerStatus;
            } else if (sendeAlarm && (neuerStatus == "Alarm" ||
                     (neuerStatus == "OK" && letzterWebhookStatus == "Alarm"))) {
                WebhookErfasseSensordaten("normal");
                millisVorherWebhook = aktuelleZeit;
                letzterWebhookStatus = neuerStatus;
            }

            vorherAlarm = aktuellerAlarmStatus;
        }

}
