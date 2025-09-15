/**
 * @file wifi.h
 * @brief WiFi connection management utilities
 * @details Provides functions for managing WiFi connectivity, including setup,
 *          connection monitoring, and signal strength measurement. Supports up
 * to 3 WiFi credentials (SSID/PASSWORD pairs) and will try all in order.
 */
#ifndef WIFI_H
#define WIFI_H

#include <ESP8266WiFi.h>

#include <functional>

#include "utils/result_types.h"

/**
 * @brief Initialize and connect to WiFi
 * @details Sets up the WiFi connection using up to 3 credentials from
 * configuration. Attempts to connect to each in order and waits until
 * connection is established or all fail.
 * @return ResourceResult Success if connected, error otherwise
 */
ResourceResult setupWiFi();

/**
 * @brief Check WiFi connection and reconnect if necessary
 * @details Monitors WiFi connection status and automatically attempts to
 *          reconnect if the connection is lost. Tries all configured
 * credentials in order.
 * @return ResourceResult Success if connected, error otherwise
 */
ResourceResult checkWiFiConnection();

/**
 * @brief Get the current WiFi signal strength (RSSI)
 * @return TypedResult<ResourceError, int> WiFi signal strength in dBm
 * @details Returns the Received Signal Strength Indicator (RSSI) value.
 *          Typical values range from -30 dBm (excellent) to -90 dBm (unusable).
 */
TypedResult<ResourceError, int> getWiFiSignalStrength();

/**
 * @brief Check if a specific port is available
 * @param port Port number to check
 * @return TypedResult<ResourceError, bool> Success with true if port is
 * available
 * @details Verifies if a network port can be used for communication.
 */
TypedResult<ResourceError, bool> checkPort(uint16_t port);

/**
 * @brief Get the index of the currently active WiFi slot (0, 1, 2), or -1 if
 * not connected
 * @return int Active WiFi slot index
 */
int getActiveWiFiSlot();

/**
 * @brief Global WiFi client instance
 * @details Used for network communications throughout the application
 */
extern WiFiClient client;

/**
 * @brief Start WiFi Access Point for manual configuration
 * @details Starts AP with HOSTNAME as SSID, no password, for manual WiFi setup.
 */
void startAPMode();

/**
 * @brief Check if AP mode is active
 * @return true if AP mode is active
 */
bool isCaptivePortalAPActive();

extern bool apModeActive;

// Forward declaration for internal WiFi credential cycling helper
bool tryAllWiFiCredentials();

/**
 * @brief Get information about WiFi connection attempts.
 * @details Returns a string with information about which SSIDs were tried
 * and their connection status.
 */
String getWiFiConnectionAttemptsInfo();

/**
 * @brief Get current WiFi connection status for live display updates.
 * @details Returns a string with the current connection attempt status.
 */
String getCurrentWiFiStatus();

/**
 * @brief Try to connect to WiFi networks with real-time display updates
 * @param displayCallback Function to call for real-time display updates
 * @return true if connection successful, false otherwise
 * @details Attempts to connect to configured WiFi networks and provides
 *          real-time feedback through the callback function.
 */
bool tryAllWiFiCredentialsWithDisplay(
    std::function<void(const String&, bool)> displayCallback);

extern int g_activeWiFiSlot;

#endif  // WIFI_H
