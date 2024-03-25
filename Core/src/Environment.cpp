#include "Environment.hpp"

namespace Core {

auto Environment::get(const std::string &key) -> std::optional<std::string> {
  for (auto &&[k, v] : environment_variables) {
    info("key: {}, value: {}", k, v);
  }

  if (environment_variables.contains(key)) {
    return environment_variables[key];
  }

  info("Key '{}' was not initialized on startup!", key);
  return std::nullopt;
}

void Environment::initialize(std::span<std::string> keys) {
  for (const auto &key : keys) {
    if (const auto value = std::getenv(key.data())) {
      set_environment_variable(key, value);
    } else {
      info("Key {} was not found.", key);
    }
  }
}

} // namespace Core
