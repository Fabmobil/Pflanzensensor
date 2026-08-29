#include "manager_types.h"

#include "configs/config.h" // Zuerst: Feature-Flags definieren

#if USE_DISPLAY
#include "manager_display.h"
#endif

#if USE_LED_TRAFFIC_LIGHT
#include "manager_led_traffic_light.h"
#endif

// Globaler System-Zustand
SystemManagerState g_managerState;

// Globale Manager-Instanzen
#if USE_DISPLAY
std::unique_ptr<DisplayManager> displayManager;
#endif
// Wenn USE_DISPLAY=false: displayManager wird in manager_display_stub.cpp definiert

#if USE_LED_TRAFFIC_LIGHT
std::unique_ptr<LedTrafficLightManager> ledTrafficLightManager;
#endif
