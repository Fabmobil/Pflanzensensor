/**
 * @file ArduinoJson.h
 * @brief ArduinoJson-Ersatz für native Unit-Tests
 *
 * sensor_autocalibration.h bindet ArduinoJson nur wegen zweier
 * Funktionsdeklarationen ein (AutoCal_from_json / AutoCal_to_json). Die Tests
 * rufen diese nicht auf, deshalb genügen unvollständige Typen - die
 * Deklarationen müssen lediglich übersetzbar sein.
 */

#ifndef NATIVE_TEST_ARDUINOJSON_H
#define NATIVE_TEST_ARDUINOJSON_H

class JsonObject;
class JsonObjectConst;
class JsonDocument;
class DynamicJsonDocument;

#endif // NATIVE_TEST_ARDUINOJSON_H
