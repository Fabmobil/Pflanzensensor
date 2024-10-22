/**
 * @file mutex.cpp
 * @brief Mutex-Unterstützung für ESP8266 (Implementierung)
 * @author Richard A Burton
 * @date 2015
 *
 * Dieses Modul bietet einfache Mutex-Funktionalität für den ESP8266,
 * um kritische Abschnitte in Multi-Tasking-Umgebungen zu schützen.
 */

#include "mutex.h"

void ICACHE_FLASH_ATTR CreateMutex(mutex_t *mutex) {
    *mutex = 1;
}

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

void ICACHE_FLASH_ATTR ReleaseMutex(mutex_t *mutex) {
    *mutex = 1;
}
