/**
 * @file logger.h
 * @brief Logger-Ersatz für native Unit-Tests
 *
 * Die Logmakros werden zu Nichts. Getestet wird das Verhalten der
 * Zustandslogik, nicht ihre Protokollausgabe - und die echte Implementierung
 * hinge an Serial, LittleFS und dem WebSocket.
 */

#ifndef NATIVE_TEST_LOGGER_H
#define NATIVE_TEST_LOGGER_H

#include <Arduino.h>

#define LOG_DEBUG(tag, msg)                                                                        \
  do {                                                                                             \
  } while (0)
#define LOG_INFO(tag, msg)                                                                         \
  do {                                                                                             \
  } while (0)
#define LOG_WARN(tag, msg)                                                                         \
  do {                                                                                             \
  } while (0)
#define LOG_ERROR(tag, msg)                                                                        \
  do {                                                                                             \
  } while (0)

#endif // NATIVE_TEST_LOGGER_H
