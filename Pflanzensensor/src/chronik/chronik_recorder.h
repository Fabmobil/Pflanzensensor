/**
 * @file chronik_recorder.h
 * @brief Bindeglied zwischen Messzyklus und Chronik
 * @details Baut aus einem fertig gemessenen Sensor den Rahmen für den
 *          ChronikStore und liefert die Kanaltabelle, die am Anfang jedes
 *          Segments steht.
 *
 *          Eigene Datei, weil der Messzyklus den Store nicht kennen darf:
 *          sensor_measurement_cycle_data_processing.cpp steht im
 *          build_src_filter von [env:native], und ein Include von LittleFS
 *          bräche die nativen Tests. Der Zyklus ruft deshalb nur einen
 *          Funktionszeiger auf, den main_sensors.cpp hierher setzt.
 */

#ifndef CHRONIK_RECORDER_H
#define CHRONIK_RECORDER_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class Sensor;

namespace ChronikRecorder {

/**
 * @brief Wird nach jedem abgeschlossenen Messzyklus gerufen
 * @details Aufhängepunkt ist das Ende von handleProcessing(): dort sind
 *          Messwerte, Rohwerte, Status und Zeitstempel konsistent. Fasst nur
 *          RAM an; geschrieben wird später aus loop().
 */
void onMeasurementDone(Sensor& sensor);

/**
 * @brief Kanaltabelle für ein neues Segment schreiben
 * @return geschriebene Bytes, 0 wenn keine Tabelle gebaut werden konnte
 */
size_t writeChannelTable(uint8_t* dst, size_t space, uint32_t epoch);

/**
 * @brief Laufende Kanalnummer eines Messkanals
 * @return 0..15 oder -1, wenn der Kanal nicht in die Tabelle passt
 * @details Die Nummer ergibt sich aus der Reihenfolge der Sensoren und ihrer
 *          Messwerte. Sie gilt nur innerhalb eines Segments - jedes Segment
 *          trägt seine eigene Tabelle, deshalb dürfen sich die Nummern nach
 *          einer Konfigurationsänderung verschieben.
 */
int channelIndexOf(const String& sensorId, size_t measurementIndex);

/**
 * @brief Wieviele Messkanäle passen nicht mehr in die Chronik?
 * @details Der Kanalindex im Rahmenkopf hat vier Bit, es lassen sich also
 *          höchstens 16 Kanäle gleichzeitig aufzeichnen. Das reicht für jede
 *          übliche Bestückung; erst ein Vollausbau (acht Analogkanäle über
 *          Multiplexer plus acht DS18B20 an einem Pin) käme darüber. Statt die
 *          überzähligen still fallen zu lassen, werden sie gezählt und auf der
 *          Chronikseite genannt.
 */
uint8_t skippedChannels();

} // namespace ChronikRecorder

#endif // CHRONIK_RECORDER_H
