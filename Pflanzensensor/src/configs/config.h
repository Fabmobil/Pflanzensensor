/**
 * @file config.h
 * @brief Configuration file for the ESP8266 based sensor system
 */

#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "2.24.3" // Software version

#ifdef CONFIG_FILE
#include CONFIG_FILE
#else
#error "No CONFIG_FILE defined. Please define it in your build environment."
#endif

#endif // CONFIG_H
