/**
 * @file chronik_store.cpp
 * @brief Umsetzung des rollenden Messwertfensters (siehe chronik_store.h)
 */

#include "chronik/chronik_store.h"

#include <LittleFS.h>

#include "logger/logger.h"
#include "managers/manager_config.h"
#include "utils/helper.h"

using namespace ChronikFormat;
using namespace ChronikSegments;

namespace {

/// Fenster für den Startscan. Ein Segment (7936 B) passt nicht in den Heap;
/// solange kein Rahmen größer werden kann als MAX_FRAME_SIZE, enthält dieses
/// Fenster aber immer einen vollständigen Rahmen, sofern einer da ist.
constexpr size_t SCAN_WINDOW = ChronikFormat::MAX_FRAME_SIZE;

/// Häppchen fürs Ausliefern. 256 B wie WebManager::serveStaticFile - der
/// ESP8266-Stack hat nur 4 KB (web_manager_static.cpp:101).
constexpr size_t STREAM_CHUNK = 256;

/// Wievielte schnelle Wiederholung dieses Starts? Format der Datei:
/// "<zaehler> <millis>", geschrieben in main_init.cpp.
uint8_t bootLoopCount() {
  File file = LittleFS.open(F("/.boot_loop"), "r");
  if (!file) {
    return 0;
  }
  const long count = file.parseInt();
  file.close();
  return (count < 0) ? 0 : (count > 255 ? 255 : static_cast<uint8_t>(count));
}

} // namespace

ChronikStore& ChronikStore::instance() {
  static ChronikStore store;
  return store;
}

void ChronikStore::begin() {
  m_lastFlush = millis();

  // Startschleifenverdacht: dann fasst die Chronik den Flash gar nicht erst an.
  // Der Startscan ist ihre einzige Tätigkeit vor dem ersten loop() und damit
  // der einzige Teil, der zu einer Schleife beitragen könnte.
  //
  // Geprüft wird der ZÄHLER in /.boot_loop, nicht die Existenz der Datei:
  // initializeSystem() legt sie bei jedem Start an (main_init.cpp:55) und
  // entfernt sie erst nach einer Weile stabiler Laufzeit. Auf die bloße
  // Existenz zu prüfen hieße, die Chronik bei jedem Start abzuschalten.
  if (bootLoopCount() >= 2) {
    LOG_WARN(F("Chronik"), F("Mehrfachstart erkannt - Chronik bleibt für diesen Start aus"));
    m_writeDisabled = true;
    m_enabled = false;
    m_budgetDirty = false; // sonst schaltete flushIfDue() sie gleich wieder an
    return;
  }

  scanSegments();
  repairNewestSegment();
  enforceBudget();

  m_enabled = m_targetSegments > 0;
  LOG_INFO(F("Chronik"), String(F("Bereit: ")) + String(m_range.count()) + F(" von ") +
                             String(m_targetSegments) + F(" Segmenten, ") +
                             String(usedBytes() / 1024) + F(" KB belegt"));
}

void ChronikStore::scanSegments() {
  m_range.reset();
  m_currentSize = 0;
  m_hasSegment = false;

  Dir dir = LittleFS.openDir(F("/"));
  while (dir.next()) {
    const String name = dir.fileName();
    const int64_t index = indexFromName(name.c_str());
    if (index < 0) {
      continue;
    }
    m_range.add(static_cast<uint32_t>(index));
    if (m_range.newest() == static_cast<uint32_t>(index)) {
      m_currentSize = dir.fileSize();
    }
  }
  m_hasSegment = !m_range.empty();
}

void ChronikStore::repairNewestSegment() {
  if (!m_hasSegment) {
    return;
  }

  char name[NAME_BUFFER];
  nameFromIndex(m_range.newest(), name);
  File file = LittleFS.open(name, "r");
  if (!file) {
    LOG_ERROR(F("Chronik"), String(F("Jüngstes Segment nicht lesbar: ")) + name);
    return;
  }

  const size_t size = file.size();
  size_t valid = 0;
  uint8_t window[SCAN_WINDOW];

  while (valid < size) {
    if (!file.seek(valid, SeekSet)) {
      break;
    }
    const size_t read = file.read(window, SCAN_WINDOW);
    if (read == 0) {
      break;
    }
    const size_t length = frameLength(window, read);
    if (length == 0) {
      break; // angeschnittener oder beschädigter Rahmen
    }
    valid += length; // frameLength liefert nie 0 Fortschritt
    yield();
  }
  file.close();

  if (valid == size) {
    return;
  }

  // Beim letzten Stromausfall wurde ein Rahmen angeschnitten. Ohne Kürzen
  // stünde die nächste Anfügung hinter Müll und der Leser müsste sich erst
  // wieder aufsynchronisieren - das kostet den Rest des Segments.
  LOG_WARN(F("Chronik"), String(F("Angeschnittener Rahmen in ")) + name + F(": kürze von ") +
                             String(size) + F(" auf ") + String(valid) + F(" Bytes"));
  File repair = LittleFS.open(name, "r+");
  if (repair) {
    if (repair.truncate(valid)) {
      m_currentSize = valid;
    } else {
      LOG_ERROR(F("Chronik"), F("Kürzen fehlgeschlagen"));
    }
    repair.close();
  }
}

uint32_t ChronikStore::usedBytes() const {
  if (m_range.empty()) {
    return 0;
  }
  return static_cast<uint32_t>(m_range.count() - 1) * ChronikBudget::SEGMENT_SIZE + m_currentSize;
}

void ChronikStore::enforceBudget() {
  FSInfo info;
  if (!LittleFS.info(info)) {
    LOG_ERROR(F("Chronik"), F("Dateisysteminformationen nicht lesbar - Budget bleibt unverändert"));
    return;
  }

  ChronikBudget::Input in;
  in.freeBytes = static_cast<uint32_t>(info.totalBytes - info.usedBytes);
  in.ownBytes = usedBytes();
  in.fileLogEnabled = ConfigMgr.isFileLoggingEnabled();

  const uint8_t target = ChronikBudget::targetSegments(in);
  if (target != m_targetSegments) {
    LOG_INFO(F("Chronik"), String(F("Budget: ")) + String(target) + F(" Segmente (frei ") +
                               String(in.freeBytes / 1024) + F(" KB, Datei-Log ") +
                               (in.fileLogEnabled ? F("an") : F("aus")) + F(")"));
  }
  m_targetSegments = target;
  m_budgetDirty = false;

  uint8_t excess = ChronikBudget::excessSegments(
      m_range.count() > 255 ? 255 : static_cast<uint8_t>(m_range.count()), m_targetSegments);
  while (excess > 0 && m_range.count() > 0) {
    char name[NAME_BUFFER];
    nameFromIndex(m_range.oldest(), name);
    if (!LittleFS.remove(name)) {
      LOG_ERROR(F("Chronik"), String(F("Ältestes Segment nicht löschbar: ")) + name);
      break;
    }
    LOG_DEBUG(F("Chronik"), String(F("Segment gelöscht: ")) + name);
    scanSegments(); // Ränder neu bestimmen; die Indizes sind lückenlos
    excess--;
    yield();
  }

  m_enabled = m_targetSegments > 0;
}

void ChronikStore::record(const SampleFrame& frame) {
  if (m_writeDisabled || !m_enabled || frame.count == 0) {
    return;
  }

  // Ohne synchronisierte Uhr gäbe es Datenpunkte aus dem Jahr 1970. Eine Lücke
  // im Diagramm ist ehrlicher als eine falsche Zeitachse.
  if (frame.epoch < MIN_PLAUSIBLE_EPOCH) {
    m_dropped++;
    const uint32_t now = millis();
    if (m_lastDropLog == 0 || (now - m_lastDropLog) > 3600000UL) {
      m_lastDropLog = now;
      LOG_WARN(F("Chronik"), String(F("Keine synchronisierte Zeit - ")) + String(m_dropped) +
                                 F(" Rahmen verworfen"));
    }
    return;
  }

  const size_t written = writeSample(frame, m_buffer + m_bufferLength, BUFFER_MAX - m_bufferLength);
  if (written == 0) {
    // Kann bei fünf Minuten Flushfrist und Minutentakt praktisch nicht
    // eintreten; gezählt wird es trotzdem, wie beim Logger.
    m_dropped++;
    return;
  }
  m_bufferLength += written;
}

void ChronikStore::flushIfDue() {
  if (m_writeDisabled) {
    m_bufferLength = 0;
    return;
  }
  if (!m_enabled && !m_budgetDirty) {
    return;
  }
  if (m_budgetDirty) {
    enforceBudget();
  }
  if (m_bufferLength == 0) {
    return;
  }

  const uint32_t now = millis();
  const bool schwelleErreicht = m_bufferLength >= FLUSH_THRESHOLD;
  const bool fristAbgelaufen = (now - m_lastFlush) >= FLUSH_INTERVAL_MS;
  if (!schwelleErreicht && !fristAbgelaufen) {
    return;
  }

  if (appendBuffer()) {
    m_lastFlush = now;
  }
}

bool ChronikStore::startNewSegment() {
  uint32_t index = m_range.nextIndex();
  char name[NAME_BUFFER];
  nameFromIndex(index, name);

  // Schutzgurt: open(..., "w") kürzt eine vorhandene Datei auf null. Stimmt der
  // Segmentstand aus irgendeinem Grund nicht mit dem Dateisystem überein,
  // würde hier stillschweigend ein volles Segment gelöscht. Lieber eine Lücke
  // in der Nummerierung als verlorene Messwerte.
  uint8_t versuche = 0;
  while (LittleFS.exists(name) && versuche < 8) {
    LOG_WARN(F("Chronik"), String(F("Segment existiert bereits: ")) + name);
    index++;
    nameFromIndex(index, name);
    versuche++;
  }

  File file = LittleFS.open(name, "w");
  if (!file) {
    LOG_ERROR(F("Chronik"), String(F("Segment nicht anlegbar: ")) + name);
    return false;
  }

  // Jedes Segment beginnt mit seiner Kanaltabelle. Damit ist es für sich
  // lesbar: keine Zuordnungsdatei im Flash, und ein später umbenannter oder
  // entfernter Sensor macht ältere Segmente nicht unbrauchbar.
  size_t tableLength = 0;
  if (m_tableProvider) {
    uint8_t table[MAX_FRAME_SIZE];
    // Helper::getCurrentTime() statt time(nullptr): der ESP hat keine gestellte
    // Systemuhr, time() liefert die Laufzeit seit dem Start. Als Kopfzeile
    // eines Segments ist dieser Zeitstempel aber genau das, woran streamFrom()
    // erkennt, ob ein Segment vor der ?seit=-Grenze liegt.
    tableLength =
        m_tableProvider(table, sizeof(table), static_cast<uint32_t>(Helper::getCurrentTime()));
    if (tableLength > 0 && file.write(table, tableLength) != tableLength) {
      LOG_ERROR(F("Chronik"), F("Kanaltabelle unvollständig geschrieben"));
      file.close();
      LittleFS.remove(name);
      return false;
    }
  }
  file.close();

  m_range.add(index);
  m_currentSize = tableLength;
  m_hasSegment = true;
  LOG_DEBUG(F("Chronik"), String(F("Neues Segment: ")) + name);

  enforceBudget();
  return true;
}

bool ChronikStore::appendBuffer() {
  if (!m_hasSegment || (m_currentSize + m_bufferLength) > ChronikBudget::SEGMENT_SIZE) {
    if (!startNewSegment()) {
      return false;
    }
    // enforceBudget() in startNewSegment() kann die Chronik abgeschaltet haben
    if (!m_enabled) {
      m_bufferLength = 0;
      return false;
    }
  }

  char name[NAME_BUFFER];
  nameFromIndex(m_range.newest(), name);
  File file = LittleFS.open(name, "a");
  if (!file) {
    LOG_ERROR(F("Chronik"), String(F("Segment nicht zum Anhängen offen: ")) + name);
    return false;
  }

  const size_t written = file.write(m_buffer, m_bufferLength);
  file.close();

  if (written != m_bufferLength) {
    // Kurzschreiben heißt in aller Regel: Dateisystem voll. Kein Wiederholen in
    // der Schleife - stattdessen Platz schaffen und beim nächsten Mal weiter.
    LOG_ERROR(F("Chronik"), String(F("Nur ")) + String(written) + F(" von ") +
                                String(m_bufferLength) + F(" Bytes geschrieben - schaffe Platz"));
    m_bufferLength = 0;
    m_budgetDirty = true;
    scanSegments();
    return false;
  }

  m_currentSize += written;
  m_written++;
  m_bufferLength = 0;
  return true;
}

uint32_t ChronikStore::firstEpochOf(uint32_t index) const {
  char name[NAME_BUFFER];
  nameFromIndex(index, name);
  File file = LittleFS.open(name, "r");
  if (!file) {
    return 0;
  }
  uint8_t head[8];
  const size_t read = file.read(head, sizeof(head));
  file.close();
  if (read < 8) {
    return 0;
  }
  // Beide Rahmentypen tragen die Epoche an derselben Stelle.
  return static_cast<uint32_t>(head[2]) | (static_cast<uint32_t>(head[3]) << 8) |
         (static_cast<uint32_t>(head[4]) << 16) | (static_cast<uint32_t>(head[5]) << 24);
}

bool ChronikStore::streamFrom(uint32_t sinceEpoch, Sink sink, void* context) {
  if (!sink) {
    return false;
  }

  if (m_hasSegment) {
    const uint32_t oldest = m_range.oldest();
    const uint32_t newest = m_range.newest();

    for (uint32_t index = oldest; index <= newest; index++) {
      char name[NAME_BUFFER];
      nameFromIndex(index, name);
      if (!LittleFS.exists(name)) {
        continue; // sollte nicht vorkommen, aber Lücken dürfen nicht abbrechen
      }

      // Ein Segment ist entbehrlich, wenn schon das nächste vor der Grenze
      // beginnt - dann liegt sein gesamter Inhalt davor. Dafür genügen die
      // ersten acht Bytes des Nachfolgers, kein Rahmen wird geparst.
      if (sinceEpoch > 0 && index < newest) {
        const uint32_t naechsteEpoche = firstEpochOf(index + 1);
        if (naechsteEpoche != 0 && naechsteEpoche <= sinceEpoch) {
          continue;
        }
      }

      File file = LittleFS.open(name, "r");
      if (!file) {
        continue;
      }
      uint8_t chunk[STREAM_CHUNK];
      while (file.available()) {
        const size_t read = file.read(chunk, STREAM_CHUNK);
        if (read == 0) {
          break;
        }
        if (!sink(context, chunk, read)) {
          file.close();
          return false;
        }
        optimistic_yield(1000);
      }
      file.close();
    }
  }

  // Die aktuelle Kanaltabelle zum Schluss, direkt vor dem RAM-Puffer.
  //
  // Zwei Gründe: solange noch kein Segment angelegt wurde - die ersten Minuten
  // nach einem Neustart -, enthielte der Strom sonst überhaupt keine Tabelle
  // und der Browser hätte nur nackte Kanalnummern. Und weil der Leser Tabellen
  // in der Reihenfolge anwendet, in der sie kommen, beschreibt die letzte
  // genau die Rahmen, die ihr folgen: die aus dem Puffer.
  if (m_tableProvider) {
    uint8_t table[MAX_FRAME_SIZE];
    const size_t length =
        m_tableProvider(table, sizeof(table), static_cast<uint32_t>(Helper::getCurrentTime()));
    if (length > 0 && !sink(context, table, length)) {
      return false;
    }
  }

  // Der RAM-Puffer gehört zum Datenbestand, darf hier aber NICHT geschrieben
  // werden: streamFrom() läuft im Webhandler, und dort ist Flash-Zugriff
  // verboten (siehe Dateikopf). Er wird deshalb einfach mitgesendet - dasselbe
  // Rahmenformat, der Leser merkt keinen Unterschied.
  if (m_bufferLength > 0) {
    return sink(context, m_buffer, m_bufferLength);
  }
  return true;
}
