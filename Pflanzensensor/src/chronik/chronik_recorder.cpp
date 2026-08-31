/**
 * @file chronik_recorder.cpp
 * @brief Umsetzung des Bindeglieds zwischen Messzyklus und Chronik
 */

#include "chronik/chronik_recorder.h"

#include <memory>

#include "chronik/chronik_store.h"
#include "logger/logger.h"
#include "managers/manager_sensor.h"
#include "sensors/sensors.h"
#include "utils/helper.h"

#if USE_ANALOG
#include "sensors/sensor_analog.h"
#endif

extern std::unique_ptr<SensorManager> sensorManager;

namespace ChronikRecorder {

namespace {

/**
 * @brief Über alle aktiven Messkanäle laufen, in stabiler Reihenfolge
 * @details Reihenfolge ist die des SensorManagers, innerhalb eines Sensors die
 *          des Messwertfelds - dieselbe Reihenfolge wie auf der Startseite und
 *          in /getLatestValues.
 */
uint8_t g_skipped = 0;

template <typename Fn> void forEachChannel(Fn callback) {
  if (!sensorManager) {
    return;
  }
  uint8_t channel = 0;
  uint8_t skipped = 0;
  for (const auto& sensor : sensorManager->getSensors()) {
    if (!sensor || !sensor->isEnabled()) {
      continue;
    }
    const SensorConfig& config = sensor->config();
    for (size_t i = 0; i < config.activeMeasurements; i++) {
      if (!config.measurements[i].enabled) {
        continue;
      }
      if (channel >= ChronikFormat::MAX_CHANNELS) {
        skipped++; // mehr Kanäle als der Kopfbyte-Index hergibt
        continue;
      }
      callback(channel, sensor.get(), i);
      channel++;
    }
  }
  g_skipped = skipped;
}

uint8_t statusCode(const String& status) {
  if (status == F("green"))
    return ChronikFormat::STATUS_GREEN;
  if (status == F("yellow"))
    return ChronikFormat::STATUS_YELLOW;
  if (status == F("red"))
    return ChronikFormat::STATUS_RED;
  if (status == F("error"))
    return ChronikFormat::STATUS_ERROR;
  if (status == F("warmup"))
    return ChronikFormat::STATUS_WARMUP;
  return ChronikFormat::STATUS_UNKNOWN;
}

} // namespace

uint8_t skippedChannels() { return g_skipped; }

int channelIndexOf(const String& sensorId, size_t measurementIndex) {
  int gefunden = -1;
  forEachChannel([&](uint8_t channel, const Sensor* sensor, size_t index) {
    if (gefunden < 0 && sensor->getId() == sensorId && index == measurementIndex) {
      gefunden = channel;
    }
  });
  return gefunden;
}

size_t writeChannelTable(uint8_t* dst, size_t space, uint32_t epoch) {
  ChronikFormat::TableBuilder builder(dst, space, epoch);
  forEachChannel([&](uint8_t channel, const Sensor* sensor, size_t index) {
    const SensorConfig& config = sensor->config();
    const MeasurementConfig& measurement = config.measurements[index];

    // Schlüssel wie in /getLatestValues und im data-sensor-Attribut der
    // Startseite: <SensorId>_<Messindex>. Damit lassen sich Chronik und
    // Live-Anzeige im Browser ohne Übersetzungstabelle zusammenführen.
    String key = sensor->getId() + "_" + String(index);

    String name = measurement.name;
    if (name.length() == 0) {
      name = sensor->getMeasurementName(index);
    }

    builder.addChannel(channel, isAnalogSensor(sensor), key.c_str(), name.c_str(),
                       measurement.unit.c_str(), measurement.limits.yellowLow,
                       measurement.limits.greenLow, measurement.limits.greenHigh,
                       measurement.limits.yellowHigh);
  });
  if (g_skipped > 0) {
    LOG_WARN(F("Chronik"),
             String(g_skipped) + F(" Messkanäle passen nicht mehr in die Chronik (Grenze: 16)"));
  }
  return builder.finish();
}

void onMeasurementDone(Sensor& sensor) {
  const time_t now = Helper::getCurrentTime();

  ChronikFormat::SampleFrame frame;
  frame.epoch = static_cast<uint32_t>(now);
  frame.count = 0;

  const MeasurementData& data = sensor.getMeasurementData();

#if USE_ANALOG
  AnalogSensor* analog = isAnalogSensor(&sensor) ? static_cast<AnalogSensor*>(&sensor) : nullptr;
#endif

  forEachChannel([&](uint8_t channel, const Sensor* kandidat, size_t index) {
    if (kandidat != &sensor || frame.count >= ChronikFormat::MAX_CHANNELS) {
      return;
    }
    if (index >= data.activeValues) {
      return;
    }
    const float value = data.values[index];
    if (isnan(value) || !isfinite(value)) {
      return; // ein ungültiger Wert ist keine Messung
    }

    ChronikFormat::ChannelValue& out = frame.values[frame.count];
    out.channel = channel;
    out.status = statusCode(sensor.getStatus(index));
    out.value = value;
#if USE_ANALOG
    if (analog) {
      // Achtung beim Deuten: getLastRawValue() ist das LETZTE Einzelsample,
      // values[index] dagegen der Mittelwert aus MEASUREMENT_AVERAGE_COUNT
      // Samples. Beide Kurven laufen deshalb nicht deckungsgleich.
      const int raw = analog->getLastRawValue(index);
      if (raw >= 0) {
        out.hasRaw = true;
        out.raw = static_cast<int16_t>(raw);
      }
    }
#endif
    frame.count++;
  });

  if (frame.count > 0) {
    ChronikStore::instance().record(frame);
  }
}

} // namespace ChronikRecorder
