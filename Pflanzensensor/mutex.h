/**
 * @file mutex.h
 * @brief Mutex-Unterstützung für ESP8266 (Header-Datei)
 * @author Richard A Burton
 * @date 2015
 *
 * Dieses Modul bietet einfache Mutex-Funktionalität für den ESP8266,
 * um kritische Abschnitte in Multi-Tasking-Umgebungen zu schützen.
 */

#ifndef MUTEX_H
#define MUTEX_H

#include <c_types.h>

typedef int32 mutex_t;

/**
 * @brief Erstellt einen neuen Mutex
 *
 * @param mutex Zeiger auf den zu erstellenden Mutex
 */
void ICACHE_FLASH_ATTR CreateMutex(mutex_t *mutex);

/**
 * @brief Versucht, einen Mutex zu erhalten
 *
 * @param mutex Zeiger auf den zu erhaltenden Mutex
 * @return bool true wenn erfolgreich, false wenn Mutex nicht frei
 */
bool ICACHE_FLASH_ATTR GetMutex(mutex_t *mutex);

/**
 * @brief Gibt einen Mutex frei
 *
 * @param mutex Zeiger auf den freizugebenden Mutex
 */
void ICACHE_FLASH_ATTR ReleaseMutex(mutex_t *mutex);

#endif // MUTEX_H
