#pragma once

#include "Logger.hpp"
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

namespace Core {

class Environment {
public:
  static void set_environment_variable(const std::string &key,
                                       const std::string &value) {
    environment_variables[key] = value;
  }

  static auto get(const std::string &key) -> std::optional<std::string> {
    for (auto &&[k, v] : environment_variables) {
      info("key: {}, value: {}", k, v);
    }

    if (environment_variables.contains(key)) {
      return environment_variables[key];
    }

    info("Key '{}' was not initialized on startup!", key);
    return std::nullopt;
  }

  static void initialize(std::span<std::string> keys) {
    for (const auto &key : keys) {
      if (const auto value = std::getenv(key.data())) {
        set_environment_variable(key, value);
      } else {
        info("Key {} was not found.", key);
      }
    }
  }

private:
  static inline std::unordered_map<std::string, std::string>
      environment_variables{};
};

} // namespace Core