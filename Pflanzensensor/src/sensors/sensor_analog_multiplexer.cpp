/**
 * @file sensor_analog_multiplexer.cpp
 * @brief Implementation of the multiplexer control class
 */

#include "sensor_analog_multiplexer.h"

#include "utils/result_types.h"

// Multiplexer control pins from config
static const uint8_t MUX_A = MULTIPLEXER_PIN_A;  // Select bit A (LSB)
static const uint8_t MUX_B = MULTIPLEXER_PIN_B;  // Select bit B
static const uint8_t MUX_C = MULTIPLEXER_PIN_C;  // Select bit C (MSB)

Multiplexer::Multiplexer()
    : m_initialized(false),
      m_switchStartTime(0),
      m_switchInProgress(false),
      m_currentChannel(-1) {
  // No pin operations in constructor - will be done in init()
}

Multiplexer::~Multiplexer() {
  // Set all pins to input mode on destruction
  if (m_initialized) {
    pinMode(MUX_A, INPUT);
    pinMode(MUX_B, INPUT);
    pinMode(MUX_C, INPUT);
  }
}

SensorResult Multiplexer::init() {
#if USE_MULTIPLEXER
  if (m_initialized) return SensorResult::success();

  try {
  logger.debug(F("Multiplexer"), F("Initialisiere Multiplexer-Pins:"));
  logger.debug(F("Multiplexer"), F("Pin A (LSB): ") + String(MUX_A));
  logger.debug(F("Multiplexer"), F("Pin B     : ") + String(MUX_B));
  logger.debug(F("Multiplexer"), F("Pin C (MSB): ") + String(MUX_C));

    // Set up the select pins as outputs
    pinMode(MUX_A, OUTPUT);
    pinMode(MUX_B, OUTPUT);
    pinMode(MUX_C, OUTPUT);

    delay(10);  // Allow pins to stabilize after mode change

    // Set initial state to all pins HIGH (111) - corresponds to sensor 1
    digitalWrite(MUX_A, HIGH);
    digitalWrite(MUX_B, HIGH);
    digitalWrite(MUX_C, HIGH);

    delay(10);  // Allow pins to stabilize after state change

    // Validate pin states - read back each pin
    bool pinAState = digitalRead(MUX_A);
    bool pinBState = digitalRead(MUX_B);
    bool pinCState = digitalRead(MUX_C);

  String binaryState =
    String(pinCState) + String(pinBState) + String(pinAState);
  logger.debug(F("Multiplexer"),
         F("Initiale Pin-Zustände (CBA): ") + binaryState);

  if (pinAState != HIGH || pinBState != HIGH || pinCState != HIGH) {
    logger.error(F("Multiplexer"),
           F("Konnte initiale Pin-Zustände nicht setzen. Erwartet: 111, erhalten: ") +
             binaryState);
      return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                                F("Failed to set initial pin states"));
    }

    m_initialized = true;
    m_currentChannel = 1;  // Initial state (111) corresponds to sensor 1
    return SensorResult::success();
  } catch (...) {
  logger.error(F("Multiplexer"), F("Ausnahme während der Initialisierung"));
    m_initialized = false;
    return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                              F("Exception during initialization"));
  }
#else
  return SensorResult::fail(SensorError::INITIALIZATION_ERROR,
                            F("Multiplexer not supported"));
#endif
}

bool Multiplexer::switchToSensor(int sensorIndex) {
#if USE_MULTIPLEXER
  if (!m_initialized) {
  logger.error(F("Multiplexer"), F("Nicht initialisiert beim Umschaltversuch"));
    return false;
  }

  // Validate sensor index (1-8)
  if (sensorIndex < 1 || sensorIndex > MAX_CHANNELS) {
  logger.error(F("Multiplexer"),
         F("Ungültiger Sensorindex: ") + String(sensorIndex) +
           F(" (gültiger Bereich: 1-") + String(MAX_CHANNELS) + F(")"));
    return false;
  }

  // If already on requested channel, return success
  if (m_currentChannel == sensorIndex) {
    return true;
  }

  // Convert sensor index to multiplexer address (inverted addressing)
  // Sensor 1 -> 111 (7), Sensor 2 -> 110 (6), ..., Sensor 8 -> 000 (0)
  int muxAddress = 7 - (sensorIndex - 1);  // This gives us the inverted address

  // Calculate individual pin states
  bool pinAState = muxAddress & 0x01;  // LSB
  bool pinBState = (muxAddress >> 1) & 0x01;
  bool pinCState = (muxAddress >> 2) & 0x01;  // MSB

  String binaryAddress =
    String(pinCState) + String(pinBState) + String(pinAState);
  logger.debug(F("Multiplexer"), F("Wechsle von Kanal ") +
                   String(m_currentChannel) + F(" zu ") +
                   String(sensorIndex) + F(" (Binär: ") +
                   binaryAddress + F(")"));

  // Set all pins at once to minimize transition time
  noInterrupts();  // Disable interrupts during pin changes
  digitalWrite(MUX_A, pinAState);
  digitalWrite(MUX_B, pinBState);
  digitalWrite(MUX_C, pinCState);
  interrupts();  // Re-enable interrupts

  delay(10);  // Short delay for pins to settle

  // Verify pin states
  if (!verifyPinStates(sensorIndex)) {
  logger.error(F("Multiplexer"),
         F("Überprüfung des Pin-Zustands fehlgeschlagen für Kanal ") +
           String(sensorIndex) + F(" - versuche erneut..."));

    // One retry attempt
    noInterrupts();
    digitalWrite(MUX_A, pinAState);
    digitalWrite(MUX_B, pinBState);
    digitalWrite(MUX_C, pinCState);
    interrupts();

    delay(10);

    if (!verifyPinStates(sensorIndex)) {
    logger.error(F("Multiplexer"),
           F("Überprüfung des Pin-Zustands erneut fehlgeschlagen für Kanal ") +
             String(sensorIndex) + F(" - gebe auf"));
      return false;
    }
  }

  // Update state
  m_currentChannel = sensorIndex;
  logger.debug(F("Multiplexer"), F("Erfolgreich auf Kanal ") +
                                     String(sensorIndex) + F(" umgeschaltet nach ") +
                                     String(millis() - m_switchStartTime) +
                                     F("ms"));
  return true;

#else
  return false;
#endif
}

bool Multiplexer::verifyPinStates(int sensorIndex) {
  // Convert sensor index to expected pin states (inverted addressing)
  int muxAddress = 7 - (sensorIndex - 1);  // This gives us the inverted address

  bool expectedA = muxAddress & 0x01;  // LSB
  bool expectedB = (muxAddress >> 1) & 0x01;
  bool expectedC = (muxAddress >> 2) & 0x01;  // MSB

  bool actualA = digitalRead(MUX_A);
  bool actualB = digitalRead(MUX_B);
  bool actualC = digitalRead(MUX_C);

  String expectedBinary =
      String(expectedC) + String(expectedB) + String(expectedA);
  String actualBinary = String(actualC) + String(actualB) + String(actualA);

  if (actualA != expectedA || actualB != expectedB || actualC != expectedC) {
    logger.error(F("Multiplexer"), F("Pin state mismatch for channel ") +
                                       String(sensorIndex) +
                                       F(" - Expected: ") + expectedBinary +
                                       F(", Got: ") + actualBinary);
    return false;
  }

  return true;
}
