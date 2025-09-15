#include "sensors/sensor_sds011.h"

#if USE_SDS011

SDS011Sensor::SDS011Sensor(const SDS011Config& config,
                           SensorManager* sensorManager)
    : Sensor(config, sensorManager),
      m_rxPin(config.rxPin),
      m_txPin(config.txPin),
      m_warmupTime(config.warmupTime),
      m_serial(std::make_unique<SoftwareSerial>(m_rxPin, m_txPin)) {
  ThresholdDefaults pm10Defaults = {0, 0, SDS011_PM10_GREEN_HIGH,
                                    SDS011_PM10_YELLOW_HIGH};
  mutableConfig().measurements[0].limits.yellowLow = pm10Defaults.yellowLow;
  mutableConfig().measurements[0].limits.greenLow = pm10Defaults.greenLow;
  mutableConfig().measurements[0].limits.greenHigh = pm10Defaults.greenHigh;
  mutableConfig().measurements[0].limits.yellowHigh = pm10Defaults.yellowHigh;
  initMeasurement(0, SDS011_PM10_NAME, SDS011_PM10_FIELD_NAME, SDS011_PM10_UNIT,
                  mutableConfig().measurements[0].limits.yellowLow,
                  mutableConfig().measurements[0].limits.greenLow,
                  mutableConfig().measurements[0].limits.greenHigh,
                  mutableConfig().measurements[0].limits.yellowHigh);

  ThresholdDefaults pm25Defaults = {0, 0, SDS011_PM25_GREEN_HIGH,
                                    SDS011_PM25_YELLOW_HIGH};
  mutableConfig().measurements[1].limits.yellowLow = pm25Defaults.yellowLow;
  mutableConfig().measurements[1].limits.greenLow = pm25Defaults.greenLow;
  mutableConfig().measurements[1].limits.greenHigh = pm25Defaults.greenHigh;
  mutableConfig().measurements[1].limits.yellowHigh = pm25Defaults.yellowHigh;
  initMeasurement(1, SDS011_PM25_NAME, SDS011_PM25_FIELD_NAME, SDS011_PM25_UNIT,
                  mutableConfig().measurements[1].limits.yellowLow,
                  mutableConfig().measurements[1].limits.greenLow,
                  mutableConfig().measurements[1].limits.greenHigh,
                  mutableConfig().measurements[1].limits.yellowHigh);
}

SDS011Sensor::~SDS011Sensor() {
  if (m_serial) {
    sleep();
  }
}

void SDS011Sensor::prepareCommand(uint8_t cmd, uint8_t data1, uint8_t data2) {
  m_command[0] = SDS011_HEAD;
  m_command[1] = SDS011_CMD_ID;
  m_command[2] = cmd;
  m_command[3] = data1;
  m_command[4] = data2;

  // Fill remaining data bytes with 0x00
  for (size_t i = 5; i < 15; i++) {
    m_command[i] = 0x00;
  }

  // Set device ID (broadcast)
  m_command[15] = 0xFF;
  m_command[16] = 0xFF;

  // Calculate checksum
  m_command[17] = calculateChecksum(m_command + 2, 15);
  m_command[18] = SDS011_TAIL;
}

SensorResult SDS011Sensor::init() {
  auto memoryResult = validateMemoryState();
  if (!memoryResult.isSuccess()) {
    return memoryResult;
  }

  logger.debug(getName(), F("Erstelle Instanz an Pins RX:") + String(m_rxPin) +
                              F(" TX:") + String(m_txPin));

  m_serial->begin(9600);
  delay(100);  // Give serial time to initialize

  // Test basic communication first
  logDebug(F("Teste Sensor-Kommunikation..."));
  if (!testCommunication()) {
    logDebug(
        F("Kommunikationstest fehlgeschlagen - Sensor möglicherweise nicht verbunden oder Pins falsch"));
    // Don't fail initialization yet, just log the warning
  } else {
    logDebug(F("Kommunikationstest erfolgreich"));
  }

  // Always keep sensor asleep initially - it will wake up during measurement
  // cycles
  logger.info(getName(), F("Sensor schläft zwischen Messungen"));

  return SensorResult::success();
}

bool SDS011Sensor::sendCommand(uint8_t cmd, uint8_t data1, uint8_t data2) {
  prepareCommand(cmd, data1, data2);

  for (size_t i = 0; i < SDS011_COMMAND_LENGTH; i++) {
    m_serial->write(m_command[i]);
  }
  m_serial->flush();

  delay(SDS011_RETRY_DELAY);
  return true;
}

SDS011Status SDS011Sensor::readResponse(unsigned long timeout) {
  unsigned long startTime = millis();
  size_t bytesRead = 0;
  uint8_t checksum = 0;

  logDebug(F("Beginne mit dem Lesen der Antwort, Timeout: ") + String(timeout) +
           F("ms"));

  while ((millis() - startTime) < timeout &&
         bytesRead < SDS011_RESPONSE_LENGTH) {
    if (m_serial->available()) {
      uint8_t b = m_serial->read();
      m_response[bytesRead] = b;

  logDebug(F("Gelesen Byte ") + String(bytesRead) + F(": 0x") +
       String(b, HEX));

      switch (bytesRead) {
        case 0:
          if (b != SDS011_HEAD) {
            logDebug(F("Ungültiges Head-Byte: erwartet 0x") +
                     String(SDS011_HEAD, HEX) + F(", erhalten 0x") + String(b, HEX));
            return SDS011Status::InvalidHead;
          }
          break;
        case 1:
          if (b != SDS011_RESPONSE_ID && b != SDS011_REPORT_ID) {
            logDebug(F("Ungültige Response-ID: erwartet 0x") +
                     String(SDS011_RESPONSE_ID, HEX) + F(" oder 0x") +
                     String(SDS011_REPORT_ID, HEX) + F(", erhalten 0x") +
                     String(b, HEX));
            return SDS011Status::InvalidResponseId;
          }
          break;
        case 2 ... 7:
          checksum += b;
          break;
        case 8:
          if (b != checksum) {
            logDebug(F("Ungültige Prüfsumme: berechnet 0x") +
                     String(checksum, HEX) + F(", erhalten 0x") + String(b, HEX));
            return SDS011Status::InvalidChecksum;
          }
          break;
        case 9:
          if (b != SDS011_TAIL) {
            logDebug(F("Ungültiges Tail-Byte: erwartet 0x") +
                     String(SDS011_TAIL, HEX) + F(", erhalten 0x") + String(b, HEX));
            return SDS011Status::InvalidTail;
          }
          break;
      }
      bytesRead++;
    }
    yield();
  }

  if (bytesRead < SDS011_RESPONSE_LENGTH) {
    logDebug(F("Timeout beim Lesen der Antwort: erhalten ") + String(bytesRead) +
             F(" Bytes, erwartet ") + String(SDS011_RESPONSE_LENGTH));
    return SDS011Status::NotAvailable;
  }

  logDebug(F("Antwort vollständig gelesen"));
  return SDS011Status::Ok;
}

/**
 * @brief Fetch a single sample for a given SDS011 measurement (0=PM10, 1=PM2.5)
 * @param value Reference to store the sample
 * @param index Measurement index
 * @return true if successful, false if hardware error
 */
bool SDS011Sensor::fetchSample(float& value, size_t index) {
  logDebug(F("Lese Probe für Index ") + String(index));
  if (!isInitialized()) {
    logDebug(F(": Versuch, Probe ohne Initialisierung zu lesen"));
    value = NAN;
    return false;
  }

  // Send query command to SDS011
  if (!sendCommand(SDS011_QUERY_CMD, 0x00, 0x00)) {
    logDebug(F(": Senden des Abfragebefehls fehlgeschlagen"));
    value = NAN;
    return false;
  }

  // Wait for and parse response
  SDS011Status status = readResponse();
  if (status != SDS011Status::Ok) {
    logDebug(F(": Konnte Antwort nicht lesen, Status: ") + String((int)status));
    value = NAN;
    return false;
  }

  // Parse PM2.5 and PM10 from response
  float pm25 = (m_response[2] + (m_response[3] << 8)) / 10.0f;
  float pm10 = (m_response[4] + (m_response[5] << 8)) / 10.0f;

  if (index == 0) {
    value = pm10;
  } else if (index == 1) {
    value = pm25;
  } else {
    value = NAN;
    return false;
  }
  logDebug(F("Gelesener Wert: ") + String(value));
  return !isnan(value);
}

void SDS011Sensor::deinitialize() {
  Sensor::deinitialize();
  m_state = {};
}

// [REMOVED: calculateAverage]
// [REMOVED: processResults]

bool SDS011Sensor::wakeup() {
  logDebug(F("Starte Lüfter zum Aufwärmen/Messen"));

  // First, check if the sensor is already awake by trying to read data
  logDebug(F("Überprüfe, ob Sensor bereits aktiv ist..."));
  m_serial->flush();  // Clear any pending data

  // Try to read any available data to see if sensor is already responding
  unsigned long startCheck = millis();
  bool hasData = false;
  while ((millis() - startCheck) < 100) {
    if (m_serial->available()) {
      uint8_t b = m_serial->read();
  logDebug(F("Gefundene vorhandene Daten: 0x") + String(b, HEX));
      hasData = true;
    }
    delay(1);
  }

  if (hasData) {
    logDebug(F("Sensor scheint bereits aktiv zu sein"));
    m_state.sleeping = false;
    return true;
  }

  // Log the command being sent
  logDebug(F("Sende Aufweck-Befehl: SLEEP_CMD=") +
           String(SDS011_SLEEP_CMD, HEX) + F(" WORK_MODE=") +
           String(SDS011_WORK_MODE, HEX));

  if (!sendCommand(SDS011_SLEEP_CMD, SDS011_WORK_MODE, 0x00)) {
    logDebug(F("Senden des Aufweck-Befehls fehlgeschlagen"));
    return false;
  }

  // Log the command that was sent
  String cmdStr = F("Sent command: ");
  for (size_t i = 0; i < SDS011_COMMAND_LENGTH; i++) {
    cmdStr += String(m_command[i], HEX) + F(" ");
  }
  logDebug(cmdStr);

  // Read and validate response
  SDS011Status status = readResponse();
  logDebug(F("Aufweck-Antwort Status: ") + String((int)status));

  if (status != SDS011Status::Ok) {
    logDebug(F("Lüfter-Startbefehl fehlgeschlagen mit Status: ") + String((int)status));
    return false;
  }

  m_state.sleeping = false;
  logDebug(F("Sensor erfolgreich aufgeweckt"));
  return true;
}

bool SDS011Sensor::sleep() {
  logDebug(F("Stoppe Lüfter zum Schutz der Sensorlebensdauer"));
  if (!sendCommand(SDS011_SLEEP_CMD, SDS011_SLEEP_MODE, 0x00)) {
    logDebug(F("Senden des Schlafbefehls fehlgeschlagen"));
    return false;
  }

  // Read and validate response
  SDS011Status status = readResponse();
  if (status != SDS011Status::Ok) {
    logDebug(F("Lüfter-Stopp-Befehl fehlgeschlagen mit Status: ") + String((int)status));
    return false;
  }

  m_state.sleeping = true;
  return true;
}

void SDS011Sensor::handleSensorError() {
  sleep();
  m_state = {};
}

uint8_t SDS011Sensor::calculateChecksum(const uint8_t* data,
                                        size_t length) const {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}

bool SDS011Sensor::requiresWarmup(unsigned long& warmupTime) const {
  warmupTime = m_warmupTime;
  return true;
}

SensorResult SDS011Sensor::startMeasurement() {
  return SensorResult::success();
}

SensorResult SDS011Sensor::continueMeasurement() {
  return SensorResult::success();
}

SensorResult SDS011Sensor::performMeasurementCycle() {
  if (!isInitialized()) {
    logger.error(
        getName(),
        F(": performMeasurementCycle called on uninitialized sensor!"));
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Sensor not initialized"));
  }

  // Always wake up the sensor before measurement
  if (m_state.sleeping) {
    logDebug(F("Waking up sensor for measurement"));
    if (!wakeup()) {
      logger.error(getName(), F("Failed to wake up sensor for measurement"));
      return SensorResult::fail(SensorError::MEASUREMENT_ERROR,
                                F("Failed to wake up sensor"));
    }
  }

  // Perform the measurement using base class implementation
  auto result = Sensor::performMeasurementCycle();

  // Always put sensor back to sleep after measurement (regardless of result)
  if (!m_state.sleeping) {
    logDebug(F("Versetze Sensor nach Messung wieder in Schlafmodus"));
    if (!sleep()) {
      logger.warning(getName(),
                       F("Konnte Sensor nach Messung nicht in Schlafmodus versetzen"));
      // Don't fail the measurement result due to sleep failure
    }
  }

  return result;
}

void SDS011Sensor::logDebugDetails() const {
  logger.debug(getName(),
               F("SDS011 state: sleeping=") + String(m_state.sleeping));
  String resp;
  for (size_t i = 0; i < SDS011_RESPONSE_LENGTH; ++i) {
    resp += String(m_response[i], HEX) + " ";
  }
  logger.debug(getName(), F("Last SDS011 response: ") + resp);
}

bool SDS011Sensor::testCommunication() {
  logDebug(F("Testing basic serial communication..."));

  // First, test if we can write to the serial port
  logDebug(F("Testing serial write capability..."));
  m_serial->write(0xAA);
  m_serial->write(0x55);
  m_serial->flush();
  logDebug(F("Sent test bytes: 0xAA 0x55"));

  // Wait a bit and check if anything comes back (echo test)
  delay(50);
  if (m_serial->available()) {
    uint8_t echo = m_serial->read();
    logDebug(F("Received echo: 0x") + String(echo, HEX));
  } else {
    logDebug(F("No echo received - this is normal for SDS011"));
  }

  // First try to wake up the sensor
  logDebug(F("Attempting to wake up sensor for communication test..."));
  if (!sendCommand(SDS011_SLEEP_CMD, SDS011_WORK_MODE, 0x00)) {
    logDebug(F("Failed to send wakeup command for test"));
    return false;
  }

  // Wait a bit for the sensor to wake up
  delay(100);

  // Send a simple query command to test if sensor responds
  if (!sendCommand(SDS011_QUERY_CMD, 0x00, 0x00)) {
    logDebug(F("Failed to send test query command"));
    return false;
  }

  // Try to read response
  SDS011Status status = readResponse(500);  // Shorter timeout for test
  logDebug(F("Test communication response status: ") + String((int)status));

  return status == SDS011Status::Ok;
}

#endif  // USE_SDS011
