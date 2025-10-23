#include "preferences_manager.h"
#include "logger/logger.h"
#include "configs/config.h"

// Ensure preference namespaces exist and populate defaults from compile-time macros
void ensureConfigNamespacesExist() {
    PreferencesManager prefsManager;

    // Initialize namespaces
    prefsManager.initNamespace("general");
    prefsManager.initNamespace("wifi");
    prefsManager.initNamespace("sensors");
    prefsManager.initNamespace("display");
    prefsManager.initNamespace("log");
    prefsManager.initNamespace("led");

    // General settings
    if (prefsManager.getString("general", "deviceName", "") == "") {
        prefsManager.setString("general", "deviceName", String(DEVICE_NAME));
        logger.info(F("Boot"), F("Standardeinstellungen: deviceName in Preferences gesetzt."));
    }

    // WiFi defaults
    if (prefsManager.getString("wifi", "ssid1", "") == "") {
        prefsManager.setString("wifi", "ssid1", String(WIFI_SSID_1));
        prefsManager.setString("wifi", "password1", String(WIFI_PASSWORD_1));
        prefsManager.setString("wifi", "ssid2", String(WIFI_SSID_2));
        prefsManager.setString("wifi", "password2", String(WIFI_PASSWORD_2));
        prefsManager.setString("wifi", "ssid3", String(WIFI_SSID_3));
        prefsManager.setString("wifi", "password3", String(WIFI_PASSWORD_3));
        logger.info(F("Boot"), F("Standardeinstellungen: WLAN-Zugangsdaten in Preferences gesetzt."));
    }

    // Display defaults
    if (prefsManager.getString("display", "show_ip", "") == "") {
        prefsManager.setString("display", "show_ip", String(true));
        prefsManager.setString("display", "show_clock", String(true));
        logger.info(F("Boot"), F("Standardeinstellungen: Display-Einstellungen in Preferences gesetzt."));
    }

    // Logging defaults
    if (prefsManager.getString("log", "file_logging_enabled", "") == "") {
        prefsManager.setString("log", "file_logging_enabled", String(FILE_LOGGING_ENABLED));
        logger.info(F("Boot"), F("Standardeinstellungen: Logging-Einstellungen in Preferences gesetzt."));
    }

    // LED defaults
    if (prefsManager.getString("led", "led_traffic_light_mode", "") == "") {
        prefsManager.setString("led", "led_traffic_light_mode", String(0));
        prefsManager.setString("led", "led_traffic_light_selected_measurement", String(""));
        logger.info(F("Boot"), F("Standardeinstellungen: LED-Ampel Einstellungen in Preferences gesetzt."));
    }
}