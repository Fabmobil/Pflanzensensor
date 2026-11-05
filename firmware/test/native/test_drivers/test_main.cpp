/**
 * @file test_main.cpp
 * @brief Treiber gegen einen Arduino-Shim: Multiplexer, LED-Ampel, MH-Z19 (ISR)
 */
#include "test_support.h" // vor ARDUINO: nativer Plattformteil

#define ARDUINO 1
// ArduinoJson soll trotz ARDUINO die Desktop-Variante nehmen
#define ARDUINOJSON_ENABLE_PROGMEM 0
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 0
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#include <Arduino.h>

#include "drivers/analog_mux.cpp"
#include "drivers/led_traffic.cpp"
#include "drivers/mhz19.cpp"

using namespace ps;

void setUp() {
  shim::reset();
  shim::muxPins[0] = board::kPins.muxA;
  shim::muxPins[1] = board::kPins.muxB;
  shim::muxPins[2] = board::kPins.muxC;
}
void tearDown() {}

void test_analog_mux_addresses_and_values() {
  // Jede Mux-Adresse liefert einen eigenen Wert: Kanal n (0-basiert) → Adresse 7-n
  for (int a = 0; a < 8; a++)
    shim::adcValue[a] = 100 + a;
  drivers::AnalogMux mux;
  TEST_ASSERT_EQUAL(Step::Ok, mux.begin(hal::now()));
  TEST_ASSERT_EQUAL(OUTPUT, shim::pinMode[board::kPins.muxA]);
  TEST_ASSERT_EQUAL(OUTPUT, shim::pinMode[board::kPins.muxC]);
  Reading out[2];
  TEST_ASSERT_EQUAL(Step::Ok, mux.pollRound(hal::now(), out, 2));
  TEST_ASSERT_EQUAL(107, out[0].raw); // Kanal 1 = A=B=C=HIGH
  TEST_ASSERT_EQUAL_FLOAT(107, out[0].value);
  TEST_ASSERT_EQUAL(106, out[1].raw);
  TEST_ASSERT_EQUAL(board::kPins.adc, shim::adcPin);
  TEST_ASSERT_EQUAL(2, shim::analogReads); // eine Runde liest alle Kanäle einmal
  TEST_ASSERT_EQUAL(2, mux.channelCount());
}

void test_analog_mux_rejects_out_of_range_and_stuck_pins() {
  drivers::AnalogMux mux;
  Reading out[2];
  TEST_ASSERT_EQUAL(Step::Error, mux.pollRound(hal::now(), out, 2)); // vor begin()
  mux.begin(hal::now());
  shim::adcValue[7] = 4000; // unmöglicher 10-Bit-Wert
  shim::adcValue[6] = -1;
  mux.pollRound(hal::now(), out, 2);
  TEST_ASSERT_TRUE(isnan(out[0].value));
  TEST_ASSERT_TRUE(isnan(out[1].value));
  // Pin bleibt hängen: Kanal wird nach einem zweiten Versuch verworfen
  shim::adcValue[7] = 500;
  shim::adcValue[6] = 300;
  shim::stuckPin = true;
  const int writes = shim::pinWrites[board::kPins.muxA];
  mux.pollRound(hal::now(), out, 2);
  TEST_ASSERT_TRUE(isnan(out[0].value));
  // Kanal 1 (A=HIGH) scheitert zweimal; Kanal 2 (A=LOW) passt zufällig zum Hänger
  TEST_ASSERT_EQUAL(writes + 2 + 1, shim::pinWrites[board::kPins.muxA]);
  TEST_ASSERT_FALSE(isnan(out[1].value));
}

void test_led_traffic_pins() {
  drivers::LedTraffic led;
  led.set(true, true, true); // vor begin(): nichts
  TEST_ASSERT_EQUAL(0, shim::pinWrites[board::kPins.ledGreen]);
  led.begin();
  TEST_ASSERT_EQUAL(OUTPUT, shim::pinMode[board::kPins.ledRed]);
  TEST_ASSERT_EQUAL(LOW, shim::pinLevel[board::kPins.ledGreen]); // aus nach begin()
  led.set(false, true, false);
  TEST_ASSERT_EQUAL(LOW, shim::pinLevel[board::kPins.ledGreen]);
  TEST_ASSERT_EQUAL(HIGH, shim::pinLevel[board::kPins.ledYellow]);
  TEST_ASSERT_EQUAL(LOW, shim::pinLevel[board::kPins.ledRed]);
}

namespace {
/// Ein PWM-Zyklus: th ms HIGH, tl ms LOW
void cycle(uint32_t& t, uint32_t thMs, uint32_t tlMs) {
  shim::edge(1, t);
  t += thMs * 1000;
  shim::edge(0, t);
  t += tlMs * 1000;
}
} // namespace

void test_mhz19_isr_measures_pwm() {
  drivers::Mhz19 co2;
  TEST_ASSERT_EQUAL(Step::Ok, co2.begin(hal::now()));
  TEST_ASSERT_EQUAL(board::kPins.mhz19Pwm, shim::isrPin);
  Reading out[1];
  co2.startRound(hal::now());
  TEST_ASSERT_EQUAL(Step::Pending, co2.pollRound(hal::now(), out, 1)); // noch kein Zyklus
  uint32_t t = 1000;
  // 1000 ppm bei range 5000: th = 2 + 1000·1000/5000 = 202 ms
  cycle(t, 202, 802);
  TEST_ASSERT_EQUAL(Step::Pending, co2.pollRound(hal::now(), out, 1)); // braucht 2 volle Zyklen
  cycle(t, 202, 802);
  cycle(t, 202, 802);
  TEST_ASSERT_EQUAL(Step::Ok, co2.pollRound(hal::now(), out, 1));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000.0f, out[0].value);
  // Kaputter Zyklus (zu kurz) ergibt keinen Wert
  co2.startRound(hal::now());
  out[0] = Reading{};
  cycle(t, 1, 100);
  cycle(t, 1, 100);
  cycle(t, 1, 100);
  TEST_ASSERT_EQUAL(Step::Ok, co2.pollRound(hal::now(), out, 1));
  TEST_ASSERT_TRUE(isnan(out[0].value));
  // begin() ist wiederholbar und hängt den Interrupt nicht doppelt an
  co2.begin(hal::now());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_analog_mux_addresses_and_values);
  RUN_TEST(test_analog_mux_rejects_out_of_range_and_stuck_pins);
  RUN_TEST(test_led_traffic_pins);
  RUN_TEST(test_mhz19_isr_measures_pwm);
  return UNITY_END();
}
