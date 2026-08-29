/**
 * @file FakeSensor.h
 * @brief Steuerbarer Sensor für Tests des echten SensorMeasurementCycleManager
 *
 * Sensor ist eine schwere Basisklasse (22 virtuelle Methoden, Konfigurations-
 * und Messdatenverwaltung), aber ihre eigentliche Implementierung
 * (sensors.cpp) hängt an nichts Hardwarespezifischem - nur an ConfigMgr,
 * Logger und ESP.getFreeHeap(), alles bereits im Shim vorhanden.
 *
 * FakeSensor überschreibt nur die reinen virtuellen Methoden und macht ihr
 * Verhalten über öffentliche Felder steuerbar. Wo nicht überschrieben, läuft
 * der ECHTE Code aus sensors.cpp - insbesondere performMeasurementCycle() mit
 * seiner Probenschleife samt Mindestverzögerung, Mittelwertbildung und
 * Fehlerzählung. Getestet wird damit die tatsächliche Produktionslogik, keine
 * Nachbildung davon.
 */

#ifndef NATIVE_TEST_FAKE_SENSOR_H
#define NATIVE_TEST_FAKE_SENSOR_H

#include <Arduino.h>

#include "sensors/sensors.h"

class FakeSensor : public Sensor {
public:
  explicit FakeSensor(const SensorConfig& config) : Sensor(config, nullptr) {}

  // ---- Steuerbares Verhalten ----

  /// Wert, den fetchSample() liefert, solange measurementsFail nicht gesetzt ist.
  float sampleValue = 42.0f;
  /// fetchSample() liefert NaN und false - lässt eine Messung im Sensor selbst
  /// scheitern (nach MAX_RETRIES=3 internen Versuchen: MEASUREMENT_ERROR).
  bool measurementsFail = false;
  /// performMeasurementCycle() liefert dauerhaft PENDING, unabhängig vom
  /// Probenzustand - simuliert einen Sensor, der in MEASURING hängen bleibt
  /// (Regressionsszenario: 30-Sekunden-Zeitschranke, siehe A1).
  bool hangInMeasuring = false;
  /// Ob und wie lange die per-Zyklus-Aufwärmphase (WARMUP-Zustand) dauert.
  bool warmupRequired = false;
  unsigned long warmupDuration = 0;
  /// Ob der Sensor nach jeder Messung deinitialisiert werden soll.
  bool deinitAfterMeasurement = true;

  /// init() zählt jeden Aufruf mit; ab initFailAfterCallNumber (1-indiziert,
  /// -1 = nie) schlägt init() fehl. Damit lassen sich reguläre
  /// Initialisierungen von einem gezielt fehlschlagenden
  /// Reinitialisierungsversuch (handleError() bei erschöpften Versuchen)
  /// unterscheiden, ohne die Aufrufstelle selbst zu kennen.
  int initCallCount = 0;
  int initFailAfterCallNumber = -1;
  int deinitCallCount = 0;

  SensorResult init() override {
    initCallCount++;
    if (initFailAfterCallNumber >= 0 && initCallCount > initFailAfterCallNumber) {
      return SensorResult::fail(SensorError::INITIALIZATION_ERROR, F("Fake-Init fehlgeschlagen"));
    }
    m_initialized = true;
    return SensorResult::success();
  }

  void deinitialize() override {
    deinitCallCount++;
    Sensor::deinitialize();
  }

  SensorResult startMeasurement() override { return performMeasurementCycle(); }
  SensorResult continueMeasurement() override { return performMeasurementCycle(); }

  SensorResult performMeasurementCycle() override {
    if (hangInMeasuring) {
      return SensorResult::fail(SensorError::PENDING, F("Fake haengt"));
    }
    // Echte Basisimplementierung: Probenschleife, Mindestverzögerung,
    // Mittelwertbildung, Fehlerzählung - siehe sensors.cpp.
    return Sensor::performMeasurementCycle();
  }

  bool fetchSample(float& value, size_t) override {
    if (measurementsFail) {
      value = NAN;
      return false;
    }
    value = sampleValue;
    return true;
  }

  bool isValidValue(float value) const override { return !isnan(value); }
  bool isValidValue(float value, size_t) const override { return !isnan(value); }

  bool requiresWarmup(unsigned long&) const override {
    return false; // Keine einmalige Aufwärmphase nach dem Einschalten
  }

  bool isMeasurementWarmupSensor() const override { return warmupRequired; }

  SensorResult startWarmup() override { return SensorResult::success(); }
  bool isWarmupComplete() const override { return true; }

  bool shouldDeinitializeAfterMeasurement() const override { return deinitAfterMeasurement; }

  SharedHardwareInfo getSharedHardwareInfo() const override {
    return SharedHardwareInfo(SensorType::DHT, 0, config().minimumDelay);
  }
};

/// Baut eine minimale, gültige SensorConfig für FakeSensor.
inline SensorConfig makeFakeSensorConfig(const char* id) {
  SensorConfig config;
  config.id = id;
  config.name = id;
  config.enabled = true;
  config.activeMeasurements = 1;
  // Realistischer Standardwert (Sensoren messen üblicherweise alle 60 s).
  // Der erste Zyklus jedes neuen SensorMeasurementCycleManager ist davon
  // unabhängig immer sofort fällig (siehe Konstruktor), erst der ZWEITE
  // Zyklus wartet dieses Intervall ab - genau das braucht ein Test, der nach
  // einem vollständigen Zyklus dauerhaft bei WAITING_FOR_DUE ankommen soll,
  // statt sofort in den nächsten Zyklus zu laufen. Tests, die schnelle
  // Wiederholungen brauchen (z.B. mehrere fehlschlagende Zyklen
  // hintereinander), setzen es gezielt auf 0 zurück.
  config.measurementInterval = 60000;
  config.minimumDelay = 0; // keine Wartezeit zwischen Proben
  config.measurements[0].name = "Testwert";
  config.measurements[0].fieldName = "testwert";
  config.measurements[0].unit = "u";
  return config;
}

#endif // NATIVE_TEST_FAKE_SENSOR_H
