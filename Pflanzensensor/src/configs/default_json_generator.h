/**
 * @file default_json_generator.h
 * @brief Functions to generate default config and sensors JSON files at boot.
 */
#ifndef DEFAULT_JSON_GENERATOR_H
#define DEFAULT_JSON_GENERATOR_H

/**
 * @brief Ensure /config.json and /sensors.json exist, creating them with macro
 * defaults if missing.
 */
void ensureConfigFilesExist();

#endif  // DEFAULT_JSON_GENERATOR_H
