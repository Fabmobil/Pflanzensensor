/**
 * @file ledampel.h
 * @brief LED-Ampel Modul für den Pflanzensensor
 * @author Tommy
 * @date 2023-09-20
 *
 * Dieses Modul enthält Funktionen zur Steuerung der LED-Ampel,
 * die visuelle Rückmeldungen über den Zustand der Pflanze gibt.
 */

#ifndef LEDAMPEL_H
#define LEDAMPEL_H

#include <Arduino.h>

extern const int ampelPinRot;
extern const int ampelPinGelb;
extern const int ampelPinGruen;

/**
 * @brief Lässt die LED-Ampel in einer bestimmten Farbe blinken
 *
 * @param farbe String; "rot", "gruen" oder "gelb"
 * @param anzahl Integer; Anzahl der Blinkvorgänge
 * @param dauer Integer; Dauer eines Blinkvorgangs in Millisekunden
 */
void LedampelBlinken(String farbe, int anzahl, int dauer);

/**
 * @brief Lässt die LED-Ampel in einer bestimmten Farbe leuchten
 *
 * @param farbe String; "rot", "gruen" oder "gelb"
 * @param dauer Integer; Dauer der Farbanzeige in Millisekunden. Bei -1 bleibt die LED an.
 */
void LedampelAnzeigen(String farbe, int dauer);

/**
 * @brief Schaltet alle LEDs der Ampel aus
 */
void LedampelAus();

#endif // LEDAMPEL_H
