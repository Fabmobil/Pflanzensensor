/**
 * @file test_support.h
 * @brief Gemeinsame Hilfen der nativen Tests
 */
#pragma once

#include <ArduinoJson.h>
#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "core/json_writer.h"
#include "platform/native/native.h"

namespace pstest {

struct StdSink : ps::Sink {
  std::string s;
  void write(const char* d, size_t n) override { s.append(d, n); }
  using ps::Sink::write;
};

/// Testvektor laden (Pfad relativ zum Projektverzeichnis)
inline bool loadVector(const char* name, JsonDocument& doc) {
  const char* dirs[] = {"test/vectors/", "../test/vectors/", "../../test/vectors/",
                        "firmware/test/vectors/"};
  for (const char* d : dirs) {
    std::ifstream f(std::string(d) + name);
    if (!f)
      continue;
    std::stringstream ss;
    ss << f.rdbuf();
    return deserializeJson(doc, ss.str()) == DeserializationError::Ok;
  }
  return false;
}

inline std::string readFile(ps::native::MemFs& fs, const char* path) {
  auto it = fs.files_.find(path);
  if (it == fs.files_.end())
    return "";
  return std::string(it->second.begin(), it->second.end());
}

inline void writeFile(ps::native::MemFs& fs, const char* path, const std::string& s) {
  fs.files_[path] = std::vector<uint8_t>(s.begin(), s.end());
}

} // namespace pstest

void setUp();
void tearDown();
