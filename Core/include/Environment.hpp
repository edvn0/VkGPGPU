#pragma once

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
    if (environment_variables.contains(key)) {
      return environment_variables[key];
    }

    return std::nullopt;
  }

  static void initialize(std::span<std::string> keys) {
    for (const auto &key : keys) {
      if (const auto value = std::getenv(key.data())) {
        environment_variables[key] = value;
      }
    }
  }

private:
  static inline std::unordered_map<std::string, std::string>
      environment_variables{};
};

} // namespace Core