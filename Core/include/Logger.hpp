#pragma once

#include <fmt/core.h>
#include <string>

namespace Core {

enum class LogLevel {
  Trace,
  Debug,
  Info,
  Error,
  None // To disable logging
};

class Logger {
public:
  static Logger &getInstance();

  void set_level(LogLevel level);
  auto get_level() const -> LogLevel;

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  template <typename... Args>
  void info(fmt::format_string<Args...> format, Args &&...args);

  template <typename... Args>
  void debug(fmt::format_string<Args...> format, Args &&...args);

  template <typename... Args>
  void trace(fmt::format_string<Args...> format, Args &&...args);

  template <typename... Args>
  void error(fmt::format_string<Args...> format, Args &&...args);

private:
  Logger();

  LogLevel current_level{LogLevel::None};
  static LogLevel get_log_level_from_environment();
};

} // namespace Core

template <typename... Args>
void info(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::getInstance().info(format, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::getInstance().debug(format, std::forward<Args>(args)...);
}

template <typename... Args>
void trace(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::getInstance().trace(format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::getInstance().error(format, std::forward<Args>(args)...);
}

#include "Logger.inl"
