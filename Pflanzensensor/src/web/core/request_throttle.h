/**
 * @file request_throttle.h
 * @brief Mindestabstand zwischen zwei Anfragen an einen offenen Endpunkt
 * @details Eigene, hardwareunabhängige Datei aus demselben Grund wie
 *          managers/manager_sensor_preemption.h: die Entscheidung "ist genug
 *          Zeit vergangen" hängt an nichts weiter als einer Zahl. Die aktuelle
 *          Zeit wird übergeben statt selbst per millis() geholt, damit der
 *          native Test die Uhr stellen kann - inklusive des Überlaufs nach
 *          49,7 Tagen, den man sonst nie zu Gesicht bekommt.
 */

#ifndef REQUEST_THROTTLE_H
#define REQUEST_THROTTLE_H

#include <Arduino.h>
#include <stdint.h>

/**
 * @class RequestThrottle
 * @brief Lässt höchstens eine Anfrage pro Mindestabstand durch
 */
class RequestThrottle {
public:
  /**
   * @param minIntervalMs Mindestabstand zwischen zwei durchgelassenen Anfragen
   */
  explicit RequestThrottle(uint32_t minIntervalMs) : m_minInterval(minIntervalMs) {}

  /**
   * @brief Anfrage anmelden
   * @param now Aktuelle Zeit (millis())
   * @return true wenn die Anfrage durchgelassen wird; nur dann wird der
   *         Zeitstempel neu gesetzt. Abgewiesene Anfragen verlängern die Sperre
   *         also nicht - sonst käme wer im Sekundentakt anklopft nie durch.
   */
  bool tryAcquire(uint32_t now) {
    if (m_hasFired && (now - m_last) < m_minInterval) {
      return false;
    }
    m_last = now;
    m_hasFired = true;
    return true;
  }

  /**
   * @brief Restzeit bis zur nächsten erlaubten Anfrage
   * @param now Aktuelle Zeit (millis())
   * @return Verbleibende Millisekunden, 0 wenn gerade frei
   */
  uint32_t remaining(uint32_t now) const {
    if (!m_hasFired) {
      return 0;
    }
    uint32_t elapsed = now - m_last;
    return elapsed >= m_minInterval ? 0 : m_minInterval - elapsed;
  }

private:
  uint32_t m_minInterval; ///< Mindestabstand in Millisekunden
  uint32_t m_last{0};     ///< Zeitstempel der letzten durchgelassenen Anfrage
  /// Ohne dieses Kennzeichen gälte der Startwert 0 als "gerade eben
  /// durchgelassen" - die erste Anfrage in den ersten Sekunden nach dem
  /// Einschalten würde fälschlich abgewiesen. Die Differenzrechnung selbst ist
  /// in uint32_t überlaufsicher: nach dem Wrap von millis() ergibt
  /// (now - m_last) weiterhin die tatsächlich vergangene Zeit. Bewusst
  /// uint32_t statt unsigned long - auf dem ESP sind beide 32 Bit, im nativen
  /// Test wäre unsigned long 64 Bit und der Wrap ließe sich dort nie prüfen.
  bool m_hasFired{false};
};

#endif // REQUEST_THROTTLE_H
