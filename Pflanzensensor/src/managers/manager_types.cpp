#include "manager_types.h"

#include "configs/config.h" // Must come first to define feature flags

#if USE_DISPLAY
#include "manager_display.h"
#endif

#if USE_LED_TRAFFIC_LIGHT
#include "manager_led_traffic_light.h"
#endif

// Initialize global system state
SystemManagerState g_managerState;

// Global manager instances
#if USE_DISPLAY
std::unique_ptr<DisplayManager> displayManager;
#endif

#if USE_LED_TRAFFIC_LIGHT
std::unique_ptr<LedTrafficLightManager> ledTrafficLightManager;
#endif
