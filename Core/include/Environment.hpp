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

  static auto get(const std::string &key) -> std::optional<std::string>;
  static void initialize(std::span<std::string> keys);

private:
  static inline std::unordered_map<std::string, std::string>
      environment_variables{};
};

} // namespace Core
