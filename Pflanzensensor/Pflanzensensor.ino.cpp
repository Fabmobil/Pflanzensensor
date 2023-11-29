# 1 "/tmp/tmpdgx16v96"
#include <Arduino.h>
# 1 "/home/tommy/Programmierung/Pflanzensensor/Pflanzensensor/Pflanzensensor.ino"
# 24 "/home/tommy/Programmierung/Pflanzensensor/Pflanzensensor/Pflanzensensor.ino"
int ModuleZaehlen();
int AnalogsensorenZaehlen();
String FarbeBerechnen(int messwert, int gruenUnten, int gruenOben, int gelbUnten, int gelbOben);

#include "Configuration.h"
#include "mutex.h"
mutex_t mutex;
void setup();
void loop();
#line 37 "/home/tommy/Programmierung/Pflanzensensor/Pflanzensensor/Pflanzensensor.ino"
void setup() {
  Serial.begin(baudrateSeriell);






  delay(1000);
  CreateMutex(&mutex);
  #if MODUL_DEBUG
    Serial.println(F("#### Start von setup()"));
  #endif
# 59 "/home/tommy/Programmierung/Pflanzensensor/Pflanzensensor/Pflanzensensor.ino"
  Serial.println(" Fabmobil Pflanzensensor, V0.2");
  module = ModuleZaehlen();
  displayseiten = AnalogsensorenZaehlen() + 6;

  #if MODUL_DEBUG
    Serial.print(F("# Anzahl Module: "));
    Serial.println(module);
    Serial.print(F("# Anzahl Displayseiten: "));
    Serial.println(displayseiten);
  #endif
  #if MODUL_LEDAMPEL
    pinMode(pinAmpelGruen, OUTPUT);
    pinMode(pinAmpelGelb, OUTPUT);
    pinMode(pinAmpelRot, OUTPUT);

    LedampelBlinken("gruen", 1, 300);
    LedampelBlinken("gelb", 1, 300);
    LedampelBlinken("rot", 1, 300);
    #if MODUL_DEBUG
      Serial.println(F("## Setup der Ledampel"));
      Serial.print(F("# PIN gruene LED:                 ")); Serial.println(pinAmpelGruen);
      Serial.print(F("# PIN gelbe LED:                  ")); Serial.println(pinAmpelGelb);
      Serial.print(F("# PIN rote LED:                   ")); Serial.println(pinAmpelRot);
      Serial.print(F("# Bodenfeuchte Skala invertiert:  ")); Serial.println(ampelBodenfeuchteInvertiert);
      Serial.print(F("# Schwellwert Bodenfeuchte grün:  ")); Serial.println(ampelBodenfeuchteGruen);
      Serial.print(F("# Schwellwert Bodenfeuchte rot:   ")); Serial.println(ampelBodenfeuchteRot);
      Serial.print(F("# Lichtstärke Skala invertiert:   ")); Serial.println(ampelHelligkeitInvertiert);
      Serial.print(F("# Schwellwert Lichtstärke grün:   ")); Serial.println(ampelHelligkeitGruen);
      Serial.print(F("# Schwellwert Lichtstärke rot:    ")); Serial.println(ampelHelligkeitRot);
    #endif
  #endif
  #if MODUL_HELLIGKEIT || MODUL_BODENFEUCHTE
    pinMode(pinAnalog, INPUT);
  #endif
  #if MODUL_MULTIPLEXER
    pinMode(pinMultiplexerA, OUTPUT);
    pinMode(pinMultiplexerB, OUTPUT);
    pinMode(pinMultiplexerC, OUTPUT);
  #else
    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);
  #endif
  #if MODUL_WIFI
    String ip = WifiSetup(wifiHostname);
  #endif
  #if MODUL_DISPLAY

    if(!display.begin(SSD1306_SWITCHCAPVCC, displayAdresse)) {
      Serial.println(F("Display konnte nicht geöffnet werden."));
    }
    display.display();
    delay(1000);
    display.clearDisplay();

  #endif
  #if MODUL_DHT

    dht.begin();
    #if MODUL_DEBUG
      Serial.println(F("## DHT Sensor intialisieren und auslesen"));
      dht.temperature().getSensor(&sensor);
      Serial.println(F("# Lufttemperatursensor"));
      Serial.print (F("# Sensortyp:       ")); Serial.println(sensor.name);
      Serial.print (F("# Treiberversion:  ")); Serial.println(sensor.version);
      Serial.print (F("# ID:              ")); Serial.println(sensor.sensor_id);
      Serial.print (F("# Maximalwert:     ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
      Serial.print (F("# Minimalwert:     ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
      Serial.print (F("# Auflösung:       ")); Serial.print(sensor.resolution); Serial.println(F("°C"));

      dht.humidity().getSensor(&sensor);
      Serial.println(F("# Luftfeuchtesensor"));
      Serial.print (F("# Sensortyp:       ")); Serial.println(sensor.name);
      Serial.print (F("# Treiberversion:  ")); Serial.println(sensor.version);
      Serial.print (F("# ID:              ")); Serial.println(sensor.sensor_id);
      Serial.print (F("# Maximalwert:     ")); Serial.print(sensor.max_value); Serial.println(F("%"));
      Serial.print (F("# Minimalwert:     ")); Serial.print(sensor.min_value); Serial.println(F("%"));
      Serial.print (F("# Auflösung:       ")); Serial.print(sensor.resolution); Serial.println(F("%"));
    #endif
  #endif
  #if MODUL_MULTIPLEXER
    digitalWrite(pinMultiplexerB, HIGH);
    digitalWrite(pinMultiplexerC, HIGH);
  #endif
}





void loop() {







  unsigned long millisAktuell = millis();
  #if MODUL_DEBUG
    Serial.println(F("############ Begin von loop() #############"));
    #if MODUL_DISPLAY
      Serial.print(F("# status: "));
      Serial.print(status);
    #endif
    Serial.print(F(", millis: "));
    Serial.println(millisAktuell);
    Serial.print(F("# IP Adresse: "));
    if ( wifiAp ) {
      Serial.println(WiFi.softAPIP());
      Serial.print(F("# Anzahl der mit dem Accesspoint verbundenen Geräte: "));
      Serial.println(WiFi.softAPgetStationNum());
    } else {
      Serial.println(WiFi.localIP());
    }
    delay(2000);
  #endif

  MDNS.update();


  if (millisAktuell - millisVorherAnalog >= intervallAnalog) {
    if (GetMutex(&mutex)) {
      millisVorherAnalog = millisAktuell;
      #if MODUL_DEBUG
        Serial.println(F("### intervallAnalog erreicht."));
      #endif

      #if MODUL_HELLIGKEIT
        std::tie(helligkeitMesswert, helligkeitMesswertProzent) =
          AnalogsensorMessen(1,1,1, helligkeitName, helligkeitMinimum, helligkeitMaximum);
        helligkeitFarbe = FarbeBerechnen(helligkeitMesswertProzent, helligkeitGruenUnten, helligkeitGruenOben, helligkeitGelbUnten, helligkeitGelbOben);
      #endif


      #if MODUL_BODENFEUCHTE
        std::tie(bodenfeuchteMesswert, bodenfeuchteMesswertProzent) =
          AnalogsensorMessen(0,1,1, bodenfeuchteName, bodenfeuchteMinimum, bodenfeuchteMaximum);
        bodenfeuchteFarbe = FarbeBerechnen(bodenfeuchteMesswertProzent, bodenfeuchteGruenUnten, bodenfeuchteGruenOben, bodenfeuchteGelbUnten, bodenfeuchteGelbOben);
      #endif


      #if MODUL_ANALOG3
        std::tie(analog3Messwert, analog3MesswertProzent) =
          AnalogsensorMessen(1,0,1, analog3Name, analog3Minimum, analog3Maximum);
        analog3Farbe = FarbeBerechnen(analog3MesswertProzent, analog3GruenUnten, analog3GruenOben, analog3GelbUnten, analog3GelbOben);
      #endif


      #if MODUL_ANALOG4
        std::tie(analog4Messwert, analog4MesswertProzent) =
          AnalogsensorMessen(0,0,1, analog4Name, analog4Minimum, analog4Maximum);
        analog4Farbe = FarbeBerechnen(analog4MesswertProzent, analog4GruenUnten, analog4GruenOben, analog4GelbUnten, analog4GelbOben);
      #endif


      #if MODUL_ANALOG5
      std::tie(analog5Messwert, analog5MesswertProzent) =
          AnalogsensorMessen(1,1,0, analog5Name, analog5Minimum, analog5Maximum);
        analog5Farbe = FarbeBerechnen(analog5MesswertProzent, analog5GruenUnten, analog5GruenOben, analog5GelbUnten, analog5GelbOben);
      #endif


      #if MODUL_ANALOG6
        std::tie(analog6Messwert, analog6MesswertProzent) =
          AnalogsensorMessen(0,1,0, analog6Name, analog6Minimum, analog6Maximum);
        analog6Farbe = FarbeBerechnen(analog6MesswertProzent, analog6GruenUnten, analog6GruenOben, analog6GelbUnten, analog6GelbOben);
      #endif


      #if MODUL_ANALOG7
        std::tie(analog7Messwert, analog7MesswertProzent) =
          AnalogsensorMessen(1,0,0, analog7Name, analog7Minimum, analog7Maximum);
        analog7Farbe = FarbeBerechnen(analog7MesswertProzent, analog7GruenUnten, analog7GruenOben, analog7GelbUnten, analog7GelbOben);
      #endif


      #if MODUL_ANALOG8
        std::tie(analog8Messwert, analog8MesswertProzent) =
          AnalogsensorMessen(0,0,0, analog8Name, analog8Minimum, analog8Maximum);
        analog8Farbe = FarbeBerechnen(analog8MesswertProzent, analog8GruenUnten, analog8GruenOben, analog8GelbUnten, analog8GelbOben);
      #endif
      #if MODUL_MULTIPLEXER
        digitalWrite(pinMultiplexerB, HIGH);
        digitalWrite(pinMultiplexerC, HIGH);
      #endif
      ReleaseMutex(&mutex);
    }
  }


  #if MODUL_DHT
    if (millisAktuell - millisVorherDht >= intervallDht) {
      #if MODUL_DEBUG
        Serial.println(F("### intervallDht erreicht."));
      #endif
      millisVorherDht = millisAktuell;
      lufttemperaturMesswert = DhtMessenLufttemperatur();
      lufttemperaturFarbe = FarbeBerechnen(lufttemperaturMesswert, lufttemperaturGruenUnten, lufttemperaturGruenOben, lufttemperaturGelbUnten, lufttemperaturGelbOben);
      luftfeuchteMesswert = DhtMessenLuftfeuchte();
      luftfeuchteFarbe = FarbeBerechnen(luftfeuchteMesswert, luftfeuchteGruenUnten, luftfeuchteGruenOben, luftfeuchteGelbUnten, luftfeuchteGelbOben);
    }
  #endif


  #if MODUL_LEDAMPEL
    if (millisAktuell - millisVorherLedampel >= intervallLedampel) {
      #if MODUL_DEBUG
        Serial.println(F("### intervallLedAmpel erreicht."));
      #endif
      millisVorherLedampel = millisAktuell;
      LedampelUmschalten(helligkeitMesswertProzent, bodenfeuchteMesswertProzent);
    }
  #endif


  #if MODUL_DISPLAY
    if (millisAktuell - millisVorherDisplay >= intervallDisplay) {
      status += 1;
      if ( status == displayseiten) {
        status = 0;
      }
      #if MODUL_DEBUG
        Serial.print(F("### intervallDisplay erreicht. status: ")); Serial.println(status);
      #endif
      millisVorherDisplay = millisAktuell;

      DisplayMesswerte();
    }
  #endif



  #if MODUL_WIFI
    if (GetMutex(&mutex)) {
      Webserver.handleClient();
      ReleaseMutex(&mutex);
    }
  #endif

  #if MODUL_DEBUG
    Serial.print(F("# millisAktuell: ")); Serial.println(millisAktuell);
    Serial.println(F("############ Ende von loop() ##############"));
    Serial.println(F(""));
  #endif
}





int ModuleZaehlen() {
    int aktiveModule = 0;
    if (MODUL_BODENFEUCHTE) aktiveModule++;
    if (MODUL_DEBUG) aktiveModule++;
    if (MODUL_DISPLAY) aktiveModule++;
    if (MODUL_DHT) aktiveModule++;
    if (MODUL_HELLIGKEIT) aktiveModule++;
    if (MODUL_IFTTT) aktiveModule++;
    if (MODUL_LEDAMPEL) aktiveModule++;
    if (MODUL_WIFI) aktiveModule++;
    if (MODUL_ANALOG3) aktiveModule++;
    if (MODUL_ANALOG4) aktiveModule++;
    if (MODUL_ANALOG5) aktiveModule++;
    if (MODUL_ANALOG6) aktiveModule++;
    if (MODUL_ANALOG7) aktiveModule++;
    if (MODUL_ANALOG8) aktiveModule++;
    return aktiveModule;
}





int AnalogsensorenZaehlen() {
  int analogsensoren = 0;
  if (MODUL_ANALOG3) analogsensoren++;
  if (MODUL_ANALOG4) analogsensoren++;
  if (MODUL_ANALOG5) analogsensoren++;
  if (MODUL_ANALOG6) analogsensoren++;
  if (MODUL_ANALOG7) analogsensoren++;
  if (MODUL_ANALOG8) analogsensoren++;
  return analogsensoren;
}





String FarbeBerechnen(int messwert, int gruenUnten, int gruenOben, int gelbUnten, int gelbOben) {
  if (messwert >= gruenUnten && messwert <= gruenOben) {
    return "gruen";
  } else if (messwert < gelbUnten || messwert > gelbOben) {
    return "rot";
  } else {
    return "gelb";
  }
}