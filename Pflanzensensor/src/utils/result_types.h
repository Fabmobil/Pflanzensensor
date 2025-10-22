/**
 * @file result_types.h
 * @brief Result and error handling types for the application
 * @details Provides a comprehensive type system for handling operation results
 *          and errors across different subsystems. Includes typed results,
 *          error enums, and error handling utilities.
 */
#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

/**
 * @class ErrorBase
 * @brief Base class for error enums
 * @details Provides a common interface for all error types,
 *          requiring string conversion functionality.
 */
class ErrorBase {
public:
  /**
   * @brief Returns the error description as a String
   * @return String containing the error description
   */
  virtual String toString() const = 0;
  virtual ~ErrorBase() = default;
};

/**
 * @enum ResourceError
 * @brief Enumeration of resource management errors
 */
enum class ResourceError {
  SUCCESS,               ///< Operation completed successfully
  PARTIAL_SUCCESS,       ///< Operation partially succeeded
  ALREADY_IN_CRITICAL,   ///< Resource is already in critical section
  INSUFFICIENT_MEMORY,   ///< Not enough memory available
  FILESYSTEM_ERROR,      ///< Error accessing filesystem
  OPERATION_FAILED,      ///< Generic operation failure
  UNKNOWN_ERROR,         ///< Unspecified error
  INVALID_STATE,         ///< Resource is in invalid state
  WIFI_ERROR,            ///< WiFi-related error
  WEBSERVER_INIT_FAILED, ///< Web server initialization failed
  CONFIG_ERROR,          ///< Configuration error
  RESOURCE_ERROR,        ///< Generic resource error
  TIME_SYNC_ERROR,       ///< Time synchronization failed
  INFLUXDB_ERROR,        ///< InfluxDB operation failed
  WEBSERVER_ERROR,       ///< Web server operation failed
  WEBSOCKET_ERROR,       ///< WebSocket operation failed
  VALIDATION_ERROR       ///< Invalid parameters or input validation failed
};

/**
 * @enum SensorError
 * @brief Enumeration of sensor-related errors
 */
enum class SensorError {
  SUCCESS,              ///< Operation completed successfully
  PARTIAL_SUCCESS,      ///< Operation partially succeeded
  INITIALIZATION_ERROR, ///< Sensor initialization failed
  VALIDATION_ERROR,     ///< Invalid sensor parameters
  MEASUREMENT_ERROR,    ///< Error during measurement
  RESOURCE_ERROR,       ///< Resource allocation error
  MEMORY_ERROR,         ///< Memory allocation or corruption error
  UNKNOWN_ERROR,        ///< Unspecified error
  CONFIG_ERROR,         ///< Configuration error
  INVALID_STATE,        ///< Sensor is in invalid state
  PENDING               ///< Measurement is in progress, not yet complete
};

/**
 * @enum ConfigError
 * @brief Enumeration of configuration-related errors
 */
enum class ConfigError {
  SUCCESS,          ///< Operation completed successfully
  VALIDATION_ERROR, ///< Invalid configuration parameters
  FILE_ERROR,       ///< Error accessing configuration file
  PARSE_ERROR,      ///< Error parsing configuration file
  UNKNOWN_ERROR,    ///< Unspecified error
  SAVE_FAILED       ///< Failed to save configuration
};

/**
 * @enum DisplayError
 * @brief Enumeration of display-related errors
 */
enum class DisplayError {
  SUCCESS,              ///< Operation completed successfully
  INITIALIZATION_ERROR, ///< Display initialization failed
  VALIDATION_ERROR,     ///< Invalid display parameters
  FILE_ERROR,           ///< Error accessing display resources
  DISPLAY_ERROR,        ///< Generic display error
  UNKNOWN_ERROR,        ///< Unspecified error
  INVALID_CONFIG,       ///< Invalid display configuration
  INVALID_STATE         ///< Invalid state for display operation
};

/**
 * @enum RouterError
 * @brief Enumeration of router-related errors
 */
enum class RouterError {
  INVALID_ROUTE,       ///< Route not found or invalid
  INVALID_HANDLER,     ///< Handler not properly configured
  DUPLICATE_ROUTE,     ///< Route already exists
  NOT_FOUND,           ///< Resource not found
  OPERATION_FAILED,    ///< Generic operation failure
  REGISTRATION_FAILED, ///< Failed to register route
  INVALID_METHOD,      ///< Invalid HTTP method
  INTERNAL_ERROR,      ///< Internal router error
  RESOURCE_ERROR,      ///< Resource allocation error
  INITIALIZATION_ERROR ///< Initialization failed
};

/**
 * @enum HandlerError
 * @brief Enumeration of request handler errors
 */
enum class HandlerError {
  INVALID_REQUEST,     ///< Invalid request parameters
  UNAUTHORIZED,        ///< Authentication required
  NOT_FOUND,           ///< Resource not found
  INTERNAL_ERROR,      ///< Internal handler error
  VALIDATION_ERROR,    ///< Invalid input parameters
  DATABASE_ERROR,      ///< Database operation error
  INITIALIZATION_ERROR ///< Handler initialization failed
};

template <typename ErrorType, typename T> class TypedResult;

/**
 * @brief Global operator for result negation
 * @tparam ErrorType Type of the error enum
 * @tparam T Type of the result data
 * @param result Result to check
 * @return true if result is not successful
 */
template <typename ErrorType, typename T> bool operator!(const TypedResult<ErrorType, T>& result) {
  return !result.isSuccess();
}

/**
 * @class TypedResult
 * @brief Generic result class for operations
 * @tparam ErrorType Type of the error enum
 * @tparam T Type of the result data
 * @details Provides a type-safe way to handle operation results,
 *          including success/failure status, error information,
 *          and optional result data.
 */
template <typename ErrorType, typename T> class TypedResult {
public:
  /**
   * @brief Creates a successful result without data
   * @return A successful TypedResult
   */
  static TypedResult success() { return TypedResult(); }

  /**
   * @brief Creates a successful result with data
   * @param data The result data
   * @return A successful TypedResult containing the data
   */
  static TypedResult success(const T& data) { return TypedResult(data); }

  /**
   * @brief Creates a failed result
   * @param error The error that occurred
   * @param message Optional error message
   * @return A failed TypedResult
   */
  static TypedResult fail(ErrorType error, const String& message = "") {
    return TypedResult(error, message);
  }

  /**
   * @brief Implicit conversion to bool
   * @return true if result is successful, false otherwise
   */
  operator bool() const { return isSuccess(); }

  /**
   * @brief Check if result is successful
   * @return true if no error is present
   */
  inline bool isSuccess() const { return !error_.has_value(); }

  /**
   * @brief Checks if the result is an error
   * @return true if operation failed
   */
  inline bool isError() const { return error_.has_value(); }

  /**
   * @brief Gets the error if any
   * @return Optional containing the error if present
   */
  inline std::optional<ErrorType> error() const { return error_; }

  /**
   * @brief Gets the error message
   * @return Reference to the error message string
   */
  inline const String& getMessage() const { return errorMessage_; }

  /**
   * @brief Gets a complete error message
   * @return String containing error type and message
   */
  String getFullErrorMessage() const {
    if (!isError())
      return "";
    String msg = errorTypeToString(error_.value());
    if (!errorMessage_.isEmpty()) {
      msg += ": " + errorMessage_;
    }
    return msg;
  }

  /**
   * @brief Executes handler on error
   * @param handler Function to execute if result is error
   * @return Reference to this result
   */
  template <typename Func> TypedResult& onError(Func&& handler) {
    if (isError()) {
      handler(*this);
    }
    return *this;
  }

  /**
   * @brief Executes handler on success
   * @param handler Function to execute if result is success
   * @return Reference to this result
   */
  template <typename Func> TypedResult& onSuccess(Func&& handler) {
    if (isSuccess()) {
      handler(*this);
    }
    return *this;
  }

private:
  std::optional<ErrorType> error_; ///< Optional error value
  String errorMessage_;            ///< Error message if any
  std::optional<T> data_;          ///< Optional result data

  TypedResult() : error_(std::nullopt), data_(std::nullopt) {}
  TypedResult(const T& data) : error_(std::nullopt), data_(data) {}
  TypedResult(ErrorType error, const String& message = "")
      : error_(error), errorMessage_(message), data_(std::nullopt) {}
};

/**
 * @class TypedResult<ErrorType, void>
 * @brief Specialization for void results
 * @tparam ErrorType Type of the error enum
 * @details Handles operations that don't return data,
 *          only success/failure status.
 */
template <typename ErrorType> class TypedResult<ErrorType, void> {
public:
  /**
   * @brief Creates a successful result
   * @return A successful TypedResult
   */
  static TypedResult success() { return TypedResult(); }

  /**
   * @brief Creates a partially successful result
   * @param message Optional message describing partial success
   * @return A partially successful TypedResult
   */
  static TypedResult partialSuccess(const String& message = "") {
    return TypedResult(ErrorType::PARTIAL_SUCCESS, message);
  }

  /**
   * @brief Creates a failed result
   * @param error The error that occurred
   * @param message Optional error message
   * @return A failed TypedResult
   */
  static TypedResult fail(ErrorType error, const String& message = "") {
    return TypedResult(error, message);
  }

  /**
   * @brief Checks if the result is successful
   * @return true if operation succeeded
   */
  bool isSuccess() const { return !error_; }

  /**
   * @brief Checks if the result is partially successful
   * @return true if operation partially succeeded
   */
  bool isPartialSuccess() const {
    return error_.has_value() && error_.value() == ErrorType::PARTIAL_SUCCESS;
  }

  /**
   * @brief Checks if the result is an error
   * @return true if operation failed
   */
  bool isError() const { return error_.has_value() && !isPartialSuccess(); }

  /**
   * @brief Gets the error if any
   * @return Optional containing the error if present
   */
  std::optional<ErrorType> error() const { return error_; }

  /**
   * @brief Gets the error message
   * @return Reference to the error message string
   */
  const String& getMessage() const { return errorMessage_; }

  /**
   * @brief Gets a complete error message
   * @return String containing error type and message
   */
  String getFullErrorMessage() const {
    if (!isError())
      return "";
    String msg = errorTypeToString(error_.value());
    if (!errorMessage_.isEmpty()) {
      msg += ": " + errorMessage_;
    }
    return msg;
  }

  /**
   * @brief Executes handler on error
   * @param handler Function to execute if result is error
   * @return Reference to this result
   */
  template <typename Func> TypedResult& onError(Func&& handler) {
    if (isError()) {
      handler(*this);
    }
    return *this;
  }

  /**
   * @brief Executes handler on success
   * @param handler Function to execute if result is success
   * @return Reference to this result
   */
  template <typename Func> TypedResult& onSuccess(Func&& handler) {
    if (isSuccess()) {
      handler(*this);
    }
    return *this;
  }

private:
  std::optional<ErrorType> error_; ///< Optional error value
  String errorMessage_;            ///< Error message if any

  TypedResult() : error_(std::nullopt) {}
  TypedResult(ErrorType error, const String& message = "")
      : error_(error), errorMessage_(message) {}
};

/**
 * @brief Converts HandlerError to string representation
 * @param error Error to convert
 * @return String representation of the error
 */
inline String errorTypeToString(HandlerError error) {
  switch (error) {
  case HandlerError::INVALID_REQUEST:
    return F("Invalid Request");
  case HandlerError::UNAUTHORIZED:
    return F("Unauthorized");
  case HandlerError::NOT_FOUND:
    return F("Not Found");
  case HandlerError::INTERNAL_ERROR:
    return F("Internal Server Error");
  case HandlerError::VALIDATION_ERROR:
    return F("Validation Error");
  case HandlerError::DATABASE_ERROR:
    return F("Database Error");
  case HandlerError::INITIALIZATION_ERROR:
    return F("Initialization Error");
  default:
    return F("Unknown Handler Error");
  }
}

inline String errorTypeToString(SensorError error) {
  switch (error) {
  case SensorError::SUCCESS:
    return F("Success");
  case SensorError::PARTIAL_SUCCESS:
    return F("Partial Success");
  case SensorError::INITIALIZATION_ERROR:
    return F("Initialization Error");
  case SensorError::VALIDATION_ERROR:
    return F("Validation Error");
  case SensorError::MEASUREMENT_ERROR:
    return F("Measurement Error");
  case SensorError::RESOURCE_ERROR:
    return F("Resource Error");
  case SensorError::MEMORY_ERROR:
    return F("Memory Error");
  case SensorError::CONFIG_ERROR:
    return F("Configuration Error");
  case SensorError::INVALID_STATE:
    return F("Invalid State");
  case SensorError::PENDING:
    return F("Pending");
  default:
    return F("Unknown Sensor Error");
  }
}

inline String errorTypeToString(ResourceError error) {
  switch (error) {
  case ResourceError::SUCCESS:
    return F("Success");
  case ResourceError::PARTIAL_SUCCESS:
    return F("Partial Success");
  case ResourceError::ALREADY_IN_CRITICAL:
    return F("Already in Critical Operation");
  case ResourceError::INSUFFICIENT_MEMORY:
    return F("Insufficient Memory");
  case ResourceError::FILESYSTEM_ERROR:
    return F("Filesystem Error");
  case ResourceError::OPERATION_FAILED:
    return F("Operation Failed");
  case ResourceError::UNKNOWN_ERROR:
    return F("Unknown Error");
  case ResourceError::INVALID_STATE:
    return F("Invalid State");
  case ResourceError::WIFI_ERROR:
    return F("WiFi Error");
  case ResourceError::WEBSERVER_INIT_FAILED:
    return F("Web Server Init Failed");
  case ResourceError::CONFIG_ERROR:
    return F("Configuration Error");
  case ResourceError::RESOURCE_ERROR:
    return F("Resource Error");
  case ResourceError::TIME_SYNC_ERROR:
    return F("Time Sync Error");
  case ResourceError::INFLUXDB_ERROR:
    return F("InfluxDB Error");
  case ResourceError::WEBSERVER_ERROR:
    return F("Web Server Error");
  case ResourceError::WEBSOCKET_ERROR:
    return F("WebSocket Error");
  case ResourceError::VALIDATION_ERROR:
    return F("Validation Error");
  default:
    return F("Unknown Resource Error");
  }
}

/**
 * @brief Converts RouterError to string representation
 * @param error Error to convert
 * @return String representation of the error
 */
inline String errorTypeToString(RouterError error) {
  switch (error) {
  case RouterError::INVALID_ROUTE:
    return F("Invalid Route");
  case RouterError::INVALID_HANDLER:
    return F("Invalid Handler");
  case RouterError::DUPLICATE_ROUTE:
    return F("Duplicate Route");
  case RouterError::NOT_FOUND:
    return F("Not Found");
  case RouterError::OPERATION_FAILED:
    return F("Operation Failed");
  case RouterError::REGISTRATION_FAILED:
    return F("Registration Failed");
  case RouterError::INVALID_METHOD:
    return F("Invalid Method");
  case RouterError::INTERNAL_ERROR:
    return F("Internal Error");
  case RouterError::RESOURCE_ERROR:
    return F("Resource Error");
  case RouterError::INITIALIZATION_ERROR:
    return F("Initialization Error");
  default:
    return F("Unknown Router Error");
  }
}

// Type aliases
using RouterResult = TypedResult<RouterError, void>;
using HandlerResult = TypedResult<HandlerError, void>;
using SensorResult = TypedResult<SensorError, void>;
using ResourceResult = TypedResult<ResourceError, void>;
using DisplayResult = TypedResult<DisplayError, void>;

/**
 * @brief Helper class for collecting multiple errors
 * @tparam ErrorType Type of the error enum
 */
template <typename ErrorType> class ErrorCollector {
public:
  /**
   * @brief Adds an error to the collection.
   * @param error The error to be added.
   * @param message An optional error message.
   */
  void addError(ErrorType error, const String& message = "") {
    errors_.push_back({error, message});
  }

  /**
   * @brief Checks if there are any errors.
   * @return true if there are errors; otherwise false.
   */
  bool hasErrors() const { return !errors_.empty(); }

  /**
   * @brief Returns the collected errors.
   * @return A reference to the collected errors.
   */
  const std::vector<std::pair<ErrorType, String>>& getErrors() const { return errors_; }

  /**
   * @brief Converts the collected errors into a TypedResult.
   * @return A TypedResult representing the first error or success.
   */
  TypedResult<ErrorType, void> toResult() const {
    if (!hasErrors()) {
      return TypedResult<ErrorType, void>::success();
    }
    const auto& firstError = errors_.front();
    return TypedResult<ErrorType, void>::fail(firstError.first, firstError.second);
  }

private:
  std::vector<std::pair<ErrorType, String>> errors_;
};
