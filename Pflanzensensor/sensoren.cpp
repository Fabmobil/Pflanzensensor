// sensoren.cpp
#include "sensoren.h"
#include "logger.h"

// DHT Hardware Singleton
DHT_Unified* DHTHardware::dht = nullptr;
uint8_t DHTHardware::referenzCount = 0;

DHT_Unified* DHTHardware::holeDHT() {
    if (!dht) {
        dht = new DHT_Unified(0, DHT11);
        dht->begin();
    }
    referenzCount++;
    return dht;
}

void DHTHardware::freigeben() {
    if (--referenzCount == 0 && dht) {
        delete dht;
        dht = nullptr;
    }
}

// Sensor Basisklasse Implementation
bool Sensor::solleMessen() {
    if (!aktiv) return false;
    unsigned long jetzt = millis();
    if (jetzt - letzteMessung >= MESS_INTERVALL) {
        letzteMessung = jetzt;
        return true;
    }
    return false;
}

bool Sensor::istAktiv() const {
    return aktiv;
}

void Sensor::setzeAktiv(bool neuerStatus) {
    if (aktiv != neuerStatus) {
        aktiv = neuerStatus;
        if (aktiv) {
            initialisiere();
        }
        logger.info(name + (aktiv ? F(" aktiviert") : F(" deaktiviert")));
    }
}

String Sensor::holeName() const {
    return name;
}

void Sensor::setzeName(const String& neuerName) {
    name = neuerName;
}

bool Sensor::istWebhookAlarmAktiv() const {
    return webhookAlarmAktiv;
}

void Sensor::setzeWebhookAlarmAktiv(bool status) {
    webhookAlarmAktiv = status;
}

String Sensor::holeFarbe() const {
    return farbe;
}

// In sensoren.cpp - Konstruktor für AnalogSensor hinzufügen:

AnalogSensor::AnalogSensor(const String& name, int sensorPin, const MultiplexerEinstellung& muxEinst)
    : Sensor(name, SensorTyp::ANALOG),
      pin(sensorPin),
      multiplexerEinstellung(muxEinst),
      minimum(900),
      maximum(380),
      gruenUnten(40),
      gruenOben(60),
      gelbUnten(20),
      gelbOben(80),
      messwert(-1),
      messwertProzent(-1) {
    // Standard-Schwellwerte werden im Konstruktor gesetzt
    // Diese können später über Webinterface/Einstellungen überschrieben werden
}

void AnalogSensor::setzeGrenzen(int min, int max) {
    minimum = min;
    maximum = max;
}

// Stelle sicher dass auch alle anderen notwendigen Methoden implementiert sind:

void AnalogSensor::berechneFarbe() {
    if (messwertProzent >= gruenUnten && messwertProzent <= gruenOben) {
        farbe = F("gruen");
    } else if (messwertProzent < gelbUnten || messwertProzent > gelbOben) {
        farbe = F("rot");
    } else {
        farbe = F("gelb");
    }
}

void AnalogSensor::initialisiere() {
    pinMode(pin, INPUT);
    pinMode(multiplexerEinstellung.pinA, OUTPUT);
    pinMode(multiplexerEinstellung.pinB, OUTPUT);
    pinMode(multiplexerEinstellung.pinC, OUTPUT);
    logger.info(F("Analogsensor ") + name + F(" initialisiert"));
}

void AnalogSensor::messe() {
    if (!solleMessen()) return;

    digitalWrite(multiplexerEinstellung.pinA, multiplexerEinstellung.a);
    digitalWrite(multiplexerEinstellung.pinB, multiplexerEinstellung.b);
    digitalWrite(multiplexerEinstellung.pinC, multiplexerEinstellung.c);
    delay(10);

    messwert = analogRead(pin);
    messwertProzent = map(messwert, minimum, maximum, 0, 100);
    messwertProzent = constrain(messwertProzent, 0, 100);

    berechneFarbe();

    logger.info(name + F(": ") + String(messwertProzent) + F("% (") +
                String(messwert) + F(" raw, ") + farbe + F(")"));
}

void AnalogSensor::speichereEinstellungen() {
    String prefix = F("analog_") + name;
    Preferences prefs;
    prefs.begin(prefix.c_str(), false);
    prefs.putBool("aktiv", aktiv);
    prefs.putInt("min", minimum);
    prefs.putInt("max", maximum);
    prefs.putInt("gruen_u", gruenUnten);
    prefs.putInt("gruen_o", gruenOben);
    prefs.putInt("gelb_u", gelbUnten);
    prefs.putInt("gelb_o", gelbOben);
    prefs.putBool("webhook", webhookAlarmAktiv);
    prefs.end();

    logger.info(F("Analogsensor ") + name + F(" Einstellungen gespeichert"));
}

void AnalogSensor::ladeEinstellungen() {
    String prefix = F("analog_") + name;
    Preferences prefs;
    prefs.begin(prefix.c_str(), true);
    aktiv = prefs.getBool("aktiv", aktiv);
    minimum = prefs.getInt("min", minimum);
    maximum = prefs.getInt("max", maximum);
    gruenUnten = prefs.getInt("gruen_u", gruenUnten);
    gruenOben = prefs.getInt("gruen_o", gruenOben);
    gelbUnten = prefs.getInt("gelb_u", gelbUnten);
    gelbOben = prefs.getInt("gelb_o", gelbOben);
    webhookAlarmAktiv = prefs.getBool("webhook", webhookAlarmAktiv);
    prefs.end();

    logger.info(F("Analogsensor ") + name + F(" Einstellungen geladen"));
}

String AnalogSensor::holeMesswertAlsString() const {
    return String(messwertProzent) + F("%");
}

void AnalogSensor::holeSchwellwerte(int& gruenU, int& gruenO, int& gelbU, int& gelbO) const {
    gruenU = gruenUnten;
    gruenO = gruenOben;
    gelbU = gelbUnten;
    gelbO = gelbOben;
}

void AnalogSensor::setzeSchwellwerte(int gruenU, int gruenO, int gelbU, int gelbO) {
    gruenUnten = gruenU;
    gruenOben = gruenO;
    gelbUnten = gelbU;
    gelbOben = gelbO;
}

// DHT Basisklasse Implementation
DHTSensorBasis::~DHTSensorBasis() {
    if (dht) {
        DHTHardware::freigeben();
    }
}

DHTSensorBasis::DHTSensorBasis(const String& name, SensorTyp sensorTyp)
    : Sensor(name, sensorTyp),
      dht(nullptr),
      messwert(-1),
      gruenUnten(0),
      gruenOben(0),
      gelbUnten(0),
      gelbOben(0) {}

void DHTSensorBasis::initialisiere() {
    dht = DHTHardware::holeDHT();
    logger.info(F("DHT Sensor ") + name + F(" initialisiert"));
}

void DHTSensorBasis::berechneFarbe() {
    if (messwert >= gruenUnten && messwert <= gruenOben) {
        farbe = F("gruen");
    } else if (messwert < gelbUnten || messwert > gelbOben) {
        farbe = F("rot");
    } else {
        farbe = F("gelb");
    }
}

void DHTSensorBasis::speichereEinstellungen() {
    String prefix = F("dht_") + name;
    Preferences prefs;
    prefs.begin(prefix.c_str(), false);
    prefs.putBool("aktiv", aktiv);
    // Schwellwerte speichern
    prefs.putInt("gruen_u", gruenUnten);
    prefs.putInt("gruen_o", gruenOben);
    prefs.putInt("gelb_u", gelbUnten);
    prefs.putInt("gelb_o", gelbOben);
    prefs.putBool("webhook", webhookAlarmAktiv);
    prefs.end();

    logger.info(F("DHT Sensor ") + name + F(" Einstellungen gespeichert"));
}

void DHTSensorBasis::ladeEinstellungen() {
    String prefix = F("dht_") + name;
    Preferences prefs;
    prefs.begin(prefix.c_str(), true);
    aktiv = prefs.getBool("aktiv", aktiv);
    // Schwellwerte laden
    gruenUnten = prefs.getInt("gruen_u", gruenUnten);
    gruenOben = prefs.getInt("gruen_o", gruenOben);
    gelbUnten = prefs.getInt("gelb_u", gelbUnten);
    gelbOben = prefs.getInt("gelb_o", gelbOben);
    webhookAlarmAktiv = prefs.getBool("webhook", webhookAlarmAktiv);
    prefs.end();

    logger.info(F("DHT Sensor ") + name + F(" Einstellungen geladen"));
}

void DHTSensorBasis::setzeSchwellwerte(int gruenU, int gruenO, int gelbU, int gelbO) {
    gruenUnten = gruenU;
    gruenOben = gruenO;
    gelbUnten = gelbU;
    gelbOben = gelbO;
}

void DHTSensorBasis::holeSchwellwerte(int& gruenU, int& gruenO, int& gelbU, int& gelbO) const {
    gruenU = gruenUnten;
    gruenO = gruenOben;
    gelbU = gelbUnten;
    gelbO = gelbOben;
}

// LufttemperaturSensor Implementation
LufttemperaturSensor::LufttemperaturSensor()
    : DHTSensorBasis(F("Lufttemperatur"), SensorTyp::LUFTTEMPERATUR) {
    setzeSchwellwerte(19, 22, 17, 24);
}

void LufttemperaturSensor::messe() {
    if (!solleMessen()) return;

    sensors_event_t event;
    dht->temperature().getEvent(&event);

    if (isnan(event.temperature)) {
        logger.error(name + F(": Messung fehlgeschlagen"));
        messwert = -1;
    } else {
        messwert = event.temperature;
        berechneFarbe();
        logger.info(name + F(": ") + String(messwert) + F("°C (") + farbe + F(")"));
    }
}

String LufttemperaturSensor::holeMesswertAlsString() const {
    if (messwert < 0) return F("Fehler");
    return String(messwert) + F("°C");
}

// LuftfeuchteSensor Implementation
LuftfeuchteSensor::LuftfeuchteSensor()
    : DHTSensorBasis(F("Luftfeuchte"), SensorTyp::LUFTFEUCHTE) {
    setzeSchwellwerte(40, 60, 20, 80);
}

void LuftfeuchteSensor::messe() {
    if (!solleMessen()) return;

    sensors_event_t event;
    dht->humidity().getEvent(&event);

    if (isnan(event.relative_humidity)) {
        logger.error(name + F(": Messung fehlgeschlagen"));
        messwert = -1;
    } else {
        messwert = event.relative_humidity;
        berechneFarbe();
        logger.info(name + F(": ") + String(messwert) + F("% (") + farbe + F(")"));
    }
}

String LuftfeuchteSensor::holeMesswertAlsString() const {
    if (messwert < 0) return F("Fehler");
    return String(messwert) + F("%");
}

// SensorManager Implementation
SensorManager::SensorManager() {
    const uint8_t MUX_PIN_A = 15;
    const uint8_t MUX_PIN_B = 2;
    const uint8_t MUX_PIN_C = 16;

    // Standardsensoren
    sensoren.push_back(new AnalogSensor(F("Bodenfeuchte"), A0,
        MultiplexerEinstellung{1, 1, 1, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));
    sensoren.push_back(new AnalogSensor(F("Helligkeit"), A0,
        MultiplexerEinstellung{0, 1, 1, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));

    // DHT Sensoren mit ihren vordefinierten Schwellwerten
    sensoren.push_back(new LufttemperaturSensor());
    sensoren.push_back(new LuftfeuchteSensor());

    // Zusätzliche Analogsensoren
    sensoren.push_back(new AnalogSensor(F("Analog3"), A0,
        MultiplexerEinstellung{1, 0, 1, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));
    sensoren.push_back(new AnalogSensor(F("Analog4"), A0,
        MultiplexerEinstellung{0, 0, 1, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));
    sensoren.push_back(new AnalogSensor(F("Analog5"), A0,
        MultiplexerEinstellung{1, 1, 0, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));
    sensoren.push_back(new AnalogSensor(F("Analog6"), A0,
        MultiplexerEinstellung{0, 1, 0, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));
    sensoren.push_back(new AnalogSensor(F("Analog7"), A0,
        MultiplexerEinstellung{1, 0, 0, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));
    sensoren.push_back(new AnalogSensor(F("Analog8"), A0,
        MultiplexerEinstellung{0, 0, 0, MUX_PIN_A, MUX_PIN_B, MUX_PIN_C}));

    logger.info(F("SensorManager initialisiert mit ") + String(sensoren.size()) + F(" Sensoren"));
}

SensorManager::~SensorManager() {
    for (auto sensor : sensoren) {
        delete sensor;
    }
    sensoren.clear();
}

void SensorManager::initialisiere() {
    for (auto sensor : sensoren) {
        sensor->ladeEinstellungen();
        if (sensor->istAktiv()) {
            sensor->initialisiere();
        }
    }
}

void SensorManager::messungenDurchfuehren() {
    for (auto sensor : sensoren) {
        if (sensor->istAktiv()) {
            sensor->messe();
        }
    }
}

void SensorManager::speichereEinstellungen() {
    for (auto sensor : sensoren) {
        sensor->speichereEinstellungen();
    }
}

Sensor* SensorManager::holeSensor(const String& name) {
    for (auto sensor : sensoren) {
        if (sensor->holeName() == name) {
            return sensor;
        }
    }
    return nullptr;
}

const std::vector<Sensor*>& SensorManager::holeSensoren() const {
    return sensoren;
}
