/**
 * @file ota_handler.h
 * @brief Interface for OTA update functionality
 */

#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include "result_types.h"
#include <Arduino.h>

/**
 * @brief Status information for OTA updates
 */
struct OTAStatus {
  bool inProgress{false};
  size_t currentProgress{0};
  size_t totalSize{0};
  String lastError;
  String expectedMD5;
};

/**
 * @brief Interface for OTA update handlers
 */
class IOTAHandler {
public:
  virtual ~IOTAHandler() = default;

  /**
   * @brief Start OTA update process
   * @param size Expected total size of update
   * @param md5 Expected MD5 hash (optional)
   * @return Result indicating success/failure
   */
  virtual TypedResult<ResourceError, void> beginUpdate(size_t size, const String& md5 = "") = 0;

  /**
   * @brief Write update data chunk
   * @param data Pointer to data buffer
   * @param len Length of data
   * @return Result indicating success/failure
   */
  virtual TypedResult<ResourceError, void> writeData(uint8_t* data, size_t len) = 0;

  /**
   * @brief Finalize update
   * @param reboot Whether to reboot after update
   * @return Result indicating success/failure
   */
  virtual TypedResult<ResourceError, void> endUpdate(bool reboot = true) = 0;

  /**
   * @brief Abort current update
   */
  virtual void abortUpdate() = 0;

  /**
   * @brief Get current update status
   * @return Current status
   */
  virtual OTAStatus getStatus() const = 0;
};

#endif // OTA_HANDLER_H
