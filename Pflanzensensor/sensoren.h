/**
 * @file sensoren.h
 * @brief Sensor-Basisklassen für den Pflanzensensor
 * @author Claude
 * @date 2024-10-23
 */

#ifndef SENSOREN_H
#define SENSOREN_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include <DHT.h>
#include <DHT_U.h>
#include "logger.h"

// Multiplexer-Einstellungen
struct MultiplexerEinstellung {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t pinA;
    uint8_t pinB;
    uint8_t pinC;
};

// Sensortypen
enum class SensorTyp {
    ANALOG,
    LUFTTEMPERATUR,
    LUFTFEUCHTE
};

/**
 * @brief Abstrakte Basisklasse für alle Sensoren
 */

class Sensor {
protected:
    bool aktiv;
    String name;
    bool webhookAlarmAktiv;
    String farbe;
    unsigned long letzteMessung;
    SensorTyp typ;
    static const unsigned long MESS_INTERVALL = 10000;

public:
    Sensor(const String& sensorName, SensorTyp sensorTyp) :
        aktiv(false),
        name(sensorName),
        webhookAlarmAktiv(false),
        farbe("rot"),
        letzteMessung(0),
        typ(sensorTyp) {}

    virtual ~Sensor() = default;

    // Getter/Setter bleiben gleich
    bool istAktiv() const;
    void setzeAktiv(bool neuerStatus);
    String holeName() const;
    void setzeName(const String& neuerName);
    bool istWebhookAlarmAktiv() const;
    void setzeWebhookAlarmAktiv(bool status);
    String holeFarbe() const;
    SensorTyp holeSensorTyp() const { return typ; }

    // Reine virtuelle Funktionen
    virtual void initialisiere() = 0;
    virtual void messe() = 0;
    virtual void speichereEinstellungen() = 0;
    virtual void ladeEinstellungen() = 0;
    virtual String holeMesswertAlsString() const = 0;
    virtual void setzeSchwellwerte(int gruenU, int gruenO, int gelbU, int gelbO) = 0;
    virtual void holeSchwellwerte(int& gruenU, int& gruenO, int& gelbU, int& gelbO) const = 0;

protected:
    bool solleMessen();
};

class AnalogSensor : public Sensor {
private:
    int pin;
    MultiplexerEinstellung multiplexerEinstellung;
    int minimum;
    int maximum;
    int gruenUnten;
    int gruenOben;
    int gelbUnten;
    int gelbOben;
    int messwert;
    int messwertProzent;

public:
    AnalogSensor(const String& name, int sensorPin, const MultiplexerEinstellung& muxEinst);

    void initialisiere() override;
    void messe() override;
    void speichereEinstellungen() override;
    void ladeEinstellungen() override;
    String holeMesswertAlsString() const override;
    void setzeSchwellwerte(int gruenU, int gruenO, int gelbU, int gelbO) override;
    void holeSchwellwerte(int& gruenU, int& gruenO, int& gelbU, int& gelbO) const override;

    void setzeGrenzen(int min, int max);
    int holeMesswert() const { return messwert; }
    int holeMesswertProzent() const { return messwertProzent; }

private:
    void berechneFarbe();
};

/**
 * @brief Hardware-Verwaltung für den DHT Sensor
 */
class DHTHardware {
private:
    static DHT_Unified* dht;
    static uint8_t referenzCount;

public:
    static DHT_Unified* holeDHT();
    static void freigeben();
};

/**
 * @brief Basisklasse für DHT-Sensoren
 */
class DHTSensorBasis : public Sensor {
protected:
    DHT_Unified* dht;
    float messwert;
    int gruenUnten;
    int gruenOben;
    int gelbUnten;
    int gelbOben;

public:
    DHTSensorBasis(const String& name, SensorTyp sensorTyp);
    virtual ~DHTSensorBasis();

    void initialisiere() override;
    void speichereEinstellungen() override;
    void ladeEinstellungen() override;
    void setzeSchwellwerte(int gruenU, int gruenO, int gelbU, int gelbO) override;
    void holeSchwellwerte(int& gruenU, int& gruenO, int& gelbU, int& gelbO) const override;

protected:
    void berechneFarbe();
};

class LufttemperaturSensor : public DHTSensorBasis {
public:
    LufttemperaturSensor();
    void messe() override;
    String holeMesswertAlsString() const override;
    float holeTemperatur() const { return messwert; }
};

class LuftfeuchteSensor : public DHTSensorBasis {
public:
    LuftfeuchteSensor();
    void messe() override;
    String holeMesswertAlsString() const override;
    float holeLuftfeuchte() const { return messwert; }
};

/**
 * @brief Zentrale Verwaltung aller Sensoren
 */
class SensorManager {
private:
    std::vector<Sensor*> sensoren;

public:
    SensorManager();
    ~SensorManager();

    void initialisiere();
    void messungenDurchfuehren();
    void speichereEinstellungen();
    Sensor* holeSensor(const String& name);
    const std::vector<Sensor*>& holeSensoren() const;
};

#endif // SENSOREN_H
