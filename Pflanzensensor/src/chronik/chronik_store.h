/**
 * @file chronik_store.h
 * @brief Rollendes Fenster der Messwerte auf dem Dateisystem
 * @details Die Chronik hängt Messrahmen an Segmentdateien im
 *          Wurzelverzeichnis an. Ist ein Segment voll, beginnt das nächste;
 *          überschreitet die Gesamtzahl das aus dem freien Platz abgeleitete
 *          Budget, wird das älteste gelöscht. Löschen statt Umkopieren: die
 *          Rotation des Datei-Logs kopiert ihren Rest um und braucht dafür
 *          kurzzeitig die anderthalbfache Dateigröße - hier ist eine Rotation
 *          ein einzelnes remove().
 *
 *          Geschrieben wird ausschließlich in flushIfDue() aus loop(). Der
 *          Grund steht ausführlich in logger.cpp:341-344: SPI-Flash-Zugriffe
 *          brauchen eingeschaltete Interrupts, und aus einem Webhandler oder
 *          SDK-Rückruf heraus verhungert dabei der WiFi-Task - Exception 2 mit
 *          anschließendem Watchdog-Reset. record() fasst deshalb nur RAM an.
 */

#ifndef CHRONIK_STORE_H
#define CHRONIK_STORE_H

#include <Arduino.h>

#include <functional>

#include "utils/chronik_budget.h"
#include "utils/chronik_format.h"
#include "utils/chronik_segments.h"

class ChronikStore {
public:
  /// Puffergröße im RAM. Bei vier Kanälen im Minutentakt sind das rund zehn
  /// Messzyklen - reichlich für die Flushfrist. Fest statt String: bei 16 KB
  /// freiem Heap hat der Schreibpfad nichts dynamisch zu allokieren.
  static constexpr size_t BUFFER_MAX = 256;
  /// Ab hier wird vorzeitig geschrieben, damit ein einzelner Rahmen nie am
  /// vollen Puffer scheitert.
  static constexpr size_t FLUSH_THRESHOLD = 192;
  /// Spätestens nach dieser Zeit wird geschrieben. Fünf Minuten sind der
  /// Preis eines Stromausfalls; dafür fällt nur alle paar Minuten ein
  /// Flash-Zugriff an.
  static constexpr uint32_t FLUSH_INTERVAL_MS = 300000;

  /// Liefert die Kanaltabelle stückweise - bei vielen Sensoren passt sie nicht
  /// in einen Rahmen. Siehe ChronikRecorder::writeChannelTable().
  /// @return geschriebene Bytes, 0 wenn nichts mehr folgt
  using TableProvider = size_t (*)(uint8_t* dst, size_t space, uint32_t epoch, uint8_t fromChannel,
                                   uint8_t* nextChannel);

  /// Nimmt die Bytes des Datenstroms entgegen. false bricht ab.
  using Sink = bool (*)(void* context, const uint8_t* data, size_t length);

  static ChronikStore& instance();

  /**
   * @brief Segmente einlesen, angeschnittenen Rahmen abschneiden, Budget setzen
   * @details Muss vor dem ersten record() laufen. Liegt /.boot_loop vor,
   *          bleibt die Chronik für diesen Start abgeschaltet - genau wie das
   *          Datei-Logging (main_init.cpp:75-85). Der Startscan ist die einzige
   *          Chronik-Tätigkeit vor dem ersten loop() und damit der einzige
   *          plausible Beitrag zu einer Startschleife.
   */
  void begin();

  void setTableProvider(TableProvider provider) { m_tableProvider = provider; }

  /// @brief Messrahmen aufnehmen. Nur RAM, kein Flash.
  void record(const ChronikFormat::SampleFrame& frame);

  /// @brief Einzige Stelle, die den Flash anfasst. Aus loop() aufrufen.
  void flushIfDue();

  /// @brief Budget beim nächsten Flush neu bestimmen (z.B. Datei-Log geändert)
  void requestBudgetRecheck() { m_budgetDirty = true; }

  /**
   * @brief Alle Rahmen ab sinceEpoch ausliefern
   * @details Segmente, die vollständig älter sind, werden übersprungen, ohne
   *          einen einzigen Rahmen zu parsen: dafür genügen die ersten acht
   *          Bytes je Segment. Der RAM-Puffer wird hinten angehängt statt
   *          geschrieben - ein Flush aus dem Webhandler ist verboten.
   */
  bool streamFrom(uint32_t sinceEpoch, Sink sink, void* context);

  bool isEnabled() const { return m_enabled && !m_writeDisabled; }
  /// Für diesen Start wegen Mehrfachstartverdacht abgeschaltet?
  bool isDisabledForBoot() const { return m_writeDisabled; }
  uint16_t segmentCount() const { return m_range.count(); }
  uint8_t targetSegments() const { return m_targetSegments; }
  uint32_t usedBytes() const;
  uint32_t droppedFrames() const { return m_dropped; }
  uint32_t framesWritten() const { return m_written; }

private:
  ChronikStore() = default;

  void scanSegments();
  void repairNewestSegment();
  void enforceBudget();
  bool appendBuffer();
  bool startNewSegment();
  /// Ruft den Rückgeber so oft auf, bis die ganze Tabelle geschrieben ist.
  /// @param write Empfänger je Rahmen; false bricht ab
  bool writeTableFrames(const std::function<bool(const uint8_t*, size_t)>& write);
  uint32_t firstEpochOf(uint32_t index) const;

  ChronikSegments::SegmentRange m_range;
  TableProvider m_tableProvider{nullptr};

  uint8_t m_buffer[BUFFER_MAX];
  size_t m_bufferLength{0};
  uint32_t m_lastFlush{0};

  uint32_t m_currentSize{0};   ///< Füllstand des jüngsten Segments
  uint8_t m_targetSegments{0}; ///< aus dem freien Platz abgeleitet
  bool m_enabled{false};
  bool m_budgetDirty{true};
  bool m_hasSegment{false};
  /// Für diesen Start hart abgeschaltet (Startschleifenverdacht). Muss von
  /// m_enabled getrennt sein: enforceBudget() setzt m_enabled anhand des
  /// freien Platzes und würde das Schreiben sonst wieder anschalten - ohne
  /// dass je ein Segmentscan gelaufen wäre.
  bool m_writeDisabled{false};

  uint32_t m_dropped{0};
  uint32_t m_written{0};
  uint32_t m_lastDropLog{0};
};

#endif // CHRONIK_STORE_H
