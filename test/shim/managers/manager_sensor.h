/**
 * @file manager_sensor.h
 * @brief SensorManager-Ersatz für native Unit-Tests
 *
 * Sensor::Sensor() nimmt einen SensorManager* entgegen, tut damit aber nichts
 * weiter - sensors.cpp ruft nie eine Methode darauf auf, das Original wird
 * nur als Zeiger gespeichert. Die echte managers/manager_sensor.h zieht über
 * sensor_factory.h sämtliche konkreten Sensortypen samt ihrer
 * Hardwarebibliotheken (DHTesp, OneWire, Adafruit ...) nach sich, die es
 * nativ nicht gibt. Ein leerer Platzhalter genügt für das, was tatsächlich
 * gebraucht wird.
 */

#ifndef NATIVE_TEST_MANAGER_SENSOR_H
#define NATIVE_TEST_MANAGER_SENSOR_H

class SensorManager {};

#endif // NATIVE_TEST_MANAGER_SENSOR_H
