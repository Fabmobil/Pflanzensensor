/**
 * @file mutex.h
 * @brief Mutex-Unterstützung für ESP8266
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

void ICACHE_FLASH_ATTR CreateMutex(mutex_t *mutex);
bool ICACHE_FLASH_ATTR GetMutex(mutex_t *mutex);
void ICACHE_FLASH_ATTR ReleaseMutex(mutex_t *mutex);

/**
 * @brief Erstellt einen neuen Mutex
 *
 * @param mutex Zeiger auf den zu erstellenden Mutex
 */
void ICACHE_FLASH_ATTR CreateMutex(mutex_t *mutex) {
	*mutex = 1;
}


/**
 * @brief Versucht, einen Mutex zu erhalten
 *
 * @param mutex Zeiger auf den zu erhaltenden Mutex
 * @return bool true wenn erfolgreich, false wenn Mutex nicht frei
 */
bool ICACHE_FLASH_ATTR GetMutex(mutex_t *mutex) {

	int iOld = 1, iNew = 0;

	asm volatile (
		"rsil a15, 1\n"    // read and set interrupt level to 1
		"l32i %0, %1, 0\n" // load value of mutex
		"bne %0, %2, 1f\n" // compare with iOld, branch if not equal
		"s32i %3, %1, 0\n" // store iNew in mutex
		"1:\n"             // branch target
		"wsr.ps a15\n"     // restore program state
		"rsync\n"
		: "=&r" (iOld)
		: "r" (mutex), "r" (iOld), "r" (iNew)
		: "a15", "memory"
	);

	return (bool)iOld;
}

/**
 * @brief Gibt einen Mutex frei
 *
 * @param mutex Zeiger auf den freizugebenden Mutex
 */
void ICACHE_FLASH_ATTR ReleaseMutex(mutex_t *mutex) {
	*mutex = 1;
}


#endif // MUTEX_H
