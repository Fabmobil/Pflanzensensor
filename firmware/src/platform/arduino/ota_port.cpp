/**
 * @file ota_port.cpp
 */
#if defined(ARDUINO)

#include "platform/arduino/ota_port.h"

#include <Arduino.h>
#include <StreamString.h>

#if defined(ARDUINO_ARCH_ESP8266)
#include <Updater.h>
#else
#include <Update.h>
#endif

namespace ps {
namespace platform {

void OtaPort::capture() {
  // Häufige Fehler verständlich; die Core-Texte sind englisch und werden
  // in err_ (64 Zeichen) mitten in der Prüfsumme abgeschnitten.
  PGM_P known = nullptr;
  switch (Update.getError()) {
  case UPDATE_ERROR_MD5:
    known = PSTR("Prüfsumme (MD5) passt nicht zur Datei");
    break;
  case UPDATE_ERROR_SPACE:
    known = PSTR("Firmware zu groß für den freien Flash");
    break;
  case UPDATE_ERROR_SIZE:
    known = PSTR("Firmware unvollständig (Größe stimmt nicht)");
    break;
  case UPDATE_ERROR_MAGIC_BYTE:
    known = PSTR("Keine gültige Firmware-Datei");
    break;
  default:
    break;
  }
  if (known) {
    strncpy_P(err_, known, sizeof(err_) - 1);
    err_[sizeof(err_) - 1] = '\0';
    return;
  }
  StreamString s;
  Update.printError(s);
  s.trim();
  strncpy(err_, s.length() ? s.c_str() : "Update-Fehler", sizeof(err_) - 1);
  err_[sizeof(err_) - 1] = '\0';
}

bool OtaPort::begin(uint32_t size, const char* md5) {
  err_[0] = '\0';
  const uint32_t room = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  if (!Update.begin(size ? size : room)) {
    capture();
    return false;
  }
  if (md5 && !Update.setMD5(md5)) {
    strncpy(err_, "MD5 ungültig", sizeof(err_) - 1);
    Update.end(false);
    return false;
  }
  return true;
}

size_t OtaPort::write(const uint8_t* data, size_t len) {
  const size_t n = Update.write(const_cast<uint8_t*>(data), len);
  if (n != len)
    capture();
  return n;
}

bool OtaPort::end() {
  if (!Update.end(true)) {
    capture();
    return false;
  }
  return true;
}

void OtaPort::abort() {
#if defined(ARDUINO_ARCH_ESP32)
  Update.abort();
#else
  Update.end(false);
#endif
}

} // namespace platform
} // namespace ps

#endif
