/**
 * @file multiplexer.h
 * @brief Multiplexer Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Steuerung des analogen Multiplexers,
 * der es ermöglicht, mehrere Analogsensoren an einem einzelnen Analogeingang zu verwenden.
 */

#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H
#include "einstellungen.h"

/**
 * @brief Schaltet den Eingang des analogen Multiplexers um
 *
 * Diese Funktion setzt die Digitaleingänge A, B und C des Multiplexers,
 * um den gewünschten Analogeingang auszuwählen.
 *
 * @param a Wert für Digitaleingang A (0 oder 1)
 * @param b Wert für Digitaleingang B (0 oder 1)
 * @param c Wert für Digitaleingang C (0 oder 1)
 */
void MultiplexerWechseln(int a, int b, int c);

#endif // MULTIPLEXER_H
