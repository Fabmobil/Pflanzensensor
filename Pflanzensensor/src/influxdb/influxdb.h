/**
 * @file influxdb.h
 * @brief InfluxDB integration for sensor data storage
 * @details Provides functionality for:
 *          - InfluxDB connection management
 *          - Measurement data transmission
 *          - System metrics reporting
 *          - Error handling and retries
 *          - Connection state management
 */

#ifndef INFLUXDB_H
#define INFLUXDB_H

#include <Arduino.h>
#include <memory>

#include "configs/config.h"
#include "utils/result_types.h"

#if USE_INFLUXDB
#include <InfluxDb.h>
#endif

// Forward declarations
class Sensor;
struct MeasurementData;

/**
 * @brief Maximum number of connection retry attempts
 * @details Used to prevent infinite retry loops and ensure
 *          graceful failure handling
 */
const int MAX_RETRIES = 3;

/**
 * @brief Delay between retry attempts in milliseconds
 * @details Provides backoff time between connection attempts
 *          to prevent overwhelming the server
 */
const int RETRY_DELAY_MS = 5000;

/**
 * @brief Global InfluxDB client instance
 * @details Singleton instance managing the InfluxDB connection:
 *          - Handles connection state
 *          - Manages authentication
 *          - Buffers data if needed
 *          - Provides thread safety
 * @note This is a global singleton instance
 */
#if USE_INFLUXDB
extern std::unique_ptr<InfluxDBClient> influxclient;
#endif

/**
 * @brief Initialize and configure InfluxDB connection
 * @return ResourceResult indicating success or failure with error details
 * @throws None
 * @details Sets up InfluxDB connection with:
 *          - Server configuration
 *          - Authentication
 *          - SSL/TLS if configured
 *          - Retry mechanism
 *          - Connection validation
 */
ResourceResult setupInfluxdb();

/**
 * @brief Send sensor measurement to InfluxDB
 * @param sensor Pointer to sensor that produced the measurement
 * @param measurementData Data structure containing measurement values
 * @return ResourceResult indicating success or failure with error details
 * @throws None
 * @details Handles measurement transmission:
 *          - Data formatting
 *          - Point creation
 *          - Error handling
 *          - Retry logic
 *          - Connection validation
 */
ResourceResult influxdbSendMeasurement(const Sensor* sensor,
                                       const MeasurementData& measurementData);

/**
 * @brief Send system metrics to InfluxDB
 * @return ResourceResult indicating success or failure with error details
 * @throws None
 * @details Transmits system information including:
 *          - Free heap memory
 *          - System uptime
 *          - Reboot count
 *          - WiFi signal strength
 *          - CPU temperature
 */
ResourceResult influxdbSendSystemInfo();

#if !USE_INFLUXDB
// When InfluxDB feature is disabled, provide inline no-op implementations
inline ResourceResult setupInfluxdb() { return ResourceResult::success(); }
inline ResourceResult influxdbSendMeasurement(const Sensor*,
                                                                                            const MeasurementData&) {
    return ResourceResult::success();
}
inline ResourceResult influxdbSendSystemInfo() {
    return ResourceResult::success();
}
#endif

#endif  // INFLUXDB_H
