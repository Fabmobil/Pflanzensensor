/**
 * @file main_init.h
 * @brief System initialization and recovery module header
 */

#ifndef MAIN_INIT_H
#define MAIN_INIT_H

#include <Arduino.h>

bool initializeSystem();
void showBootProgress(const String& message, bool clearPrevious = false);

#endif // MAIN_INIT_H
