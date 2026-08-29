/**
 * @file sensor_bmp280.h
 * @brief BMP280-Sensor für Temperatur und Luftdruck (I2C)
 */

#ifndef SENSOR_BMP280_H
#define SENSOR_BMP280_H

#include "sensors/sensors.h"

#if USE_BMP280

#include <Adafruit_BMP280.h>

/**
 * @brief Konfiguration für den BMP280-Sensor
 */
struct BMP280Config : public SensorConfig {
  uint8_t sckPin; ///< SCK-Pin (I2C SCL)
  uint8_t sdiPin; ///< SDI-Pin (I2C SDA)

  BMP280Config() : sckPin(BMP280_SCK_PIN), sdiPin(BMP280_SDI_PIN) {
    name = F("BMP280");
    id = F("BMP280");
    activeMeasurements = 2;
    if (measurementInterval == 0)
      measurementInterval = BMP280_MEASUREMENT_INTERVAL * 1000;
    minimumDelay = BMP280_MINIMUM_DELAY;
  }
};

/**
 * @brief Sensor-Klasse für BMP280 (Temperatur + Luftdruck)
 */
class BMP280Sensor : public Sensor {
public:
  explicit BMP280Sensor(const BMP280Config& config, class SensorManager* sensorManager);
  ~BMP280Sensor() override;

  SensorResult init() override;
  SensorResult startMeasurement() override;
  SensorResult continueMeasurement() override;
  void deinitialize() override;

  bool isValidValue(float value) const override { return !isnan(value); }
  bool isValidValue(float value, size_t measurementIndex) const override;
  SharedHardwareInfo getSharedHardwareInfo() const override;
  bool fetchSample(float& value, size_t index) override;

protected:
  size_t getNumMeasurements() const override { return 2; }

private:
  static constexpr uint8_t BMP280_I2C_ADDRESS = 0x76;

  const BMP280Config m_bmp280Config;
  std::unique_ptr<Adafruit_BMP280> m_bmp280;

  struct MeasurementState {
    unsigned long lastHardwareAccess{0};
    bool readInProgress{false};
    void reset() {
      readInProgress = false;
      lastHardwareAccess = 0;
    }
  } m_state;

  const BMP280Config& bmp280Config() const { return static_cast<const BMP280Config&>(config()); }
  bool canAccessHardware() const;
  bool validateReading(float value, bool isTemperature) const;
  void logDebugDetails() const override;
};

#endif // USE_BMP280
#endif // SENSOR_BMP280_H
