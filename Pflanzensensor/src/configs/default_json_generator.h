/**
 * @file default_json_generator.h
 * @brief Functions to generate default config and sensors JSON files at boot.
 */
#ifndef DEFAULT_JSON_GENERATOR_H
#define DEFAULT_JSON_GENERATOR_H

/**
 * @brief Ensure configuration exists, creating it with defaults if missing.
 * Initializes Preferences-based storage with compile-time defaults.
 */
void ensureConfigFilesExist();

/**
 * @brief Ensure sensors.json exists with default sensor configurations
 */
void ensureSensorsJsonExists();

/**
 * @brief Create legacy JSON config file (fallback only)
 */
void createLegacyConfigFiles();

#endif // DEFAULT_JSON_GENERATOR_H
