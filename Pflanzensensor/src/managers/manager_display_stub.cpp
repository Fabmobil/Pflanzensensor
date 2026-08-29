/**
 * @file manager_display_stub.cpp
 * @brief Stub-Implementierung für DisplayManager wenn USE_DISPLAY=false
 *
 * Stellt leere Methodenkörper bereit damit der Linker alle Referenzen
 * auf DisplayManager auflösen kann, ohne echten Display-Code zu kompilieren.
 */

#include "managers/manager_display.h"

#if !USE_DISPLAY

// Leere Definitionen damit der Linker vtable-Referenzen auflösen kann.
// Diese Datei ersetzt manager_display.cpp wenn USE_DISPLAY=false.

std::unique_ptr<DisplayManager> displayManager;

#endif // !USE_DISPLAY
