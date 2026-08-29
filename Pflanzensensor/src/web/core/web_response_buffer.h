/**
 * @file web_response_buffer.h
 * @brief HTTP-Response-Pufferung für effizientere Ausgabe
 * @details Sammelt mehrere sendChunk()-Aufrufe und sendet sie
 *          gebündelt. Reduziert die Anzahl der HTTP-Write-Operationen.
 * 
 * Vorteile:
 * - Weniger HTTP-Write-Operationen
 * - Bessere Performance bei vielen kleinen Chunks
 * - Flexibel: Buffering kann ein/ausgeschaltet werden
 */

#ifndef WEB_RESPONSE_BUFFER_H
#define WEB_RESPONSE_BUFFER_H

#include "utils/platform_compat.h"
#include <Arduino.h>

/**
 * @class ResponseBuffer
 * @brief Puffer für HTTP-Response-Ausgabe
 * @details Sammelt Chunks und flushed auf Anfrage.
 *          Kann als lokale Variable in Handlern verwendet werden.
 */
class ResponseBuffer {
public:
  /**
   * @brief Konstruktor
   * @param server ESPWebServer-Referenz
   * @param bufferSize Größe des internen Puffers (Default: 1024)
   */
  ResponseBuffer(ESPWebServer& server, size_t bufferSize = 1024);

  /**
   * @brief Chunk zum Puffer hinzufügen
   * @param chunk Hinzuzufügender String
   * @details Wenn Puffer voll, wird er geflusht bevor Chunk hinzugefügt
   */
  void add(const String& chunk);

  /**
   * @brief Kurzform für add()
   */
  ResponseBuffer& operator<<(const String& chunk) {
    add(chunk);
    return *this;
  }

  /**
   * @brief Puffer leeren und an Client senden
   * @details Sendet alle gesammelten Daten und leert den Puffer
   */
  void flush();

  /**
   * @brief Puffer leeren ohne zu senden (verwerfen)
   */
  void clear();

  /**
   * @brief Aktuelle Puffergröße abrufen
   */
  size_t size() const { return m_buffer.length(); }

  /**
   * @brief Prüfen ob Puffer leer ist
   */
  bool isEmpty() const { return m_buffer.isEmpty(); }

  /**
   * @brief Automatisches Flush im Destruktor
   */
  ~ResponseBuffer();

private:
  ESPWebServer& m_server;
  String m_buffer;
  size_t m_bufferSize;
};

#endif // WEB_RESPONSE_BUFFER_H