/**
 * @file web_response_buffer.cpp
 * @brief Implementierung des Response-Puffers
 */

#include "web/core/web_response_buffer.h"

ResponseBuffer::ResponseBuffer(ESPWebServer& server, size_t bufferSize)
    : m_server(server), m_bufferSize(bufferSize) {}

void ResponseBuffer::add(const String& chunk) {
  // Wenn Puffer zu groß wird, flushen
  if (m_buffer.length() + chunk.length() > m_bufferSize) {
    flush();
  }
  m_buffer += chunk;
}

void ResponseBuffer::flush() {
  if (!m_buffer.isEmpty()) {
    m_server.sendContent(m_buffer);
    m_buffer = "";
  }
}

void ResponseBuffer::clear() { m_buffer = ""; }

ResponseBuffer::~ResponseBuffer() {
  flush(); // Automatisch flushen beim Zerstören
}