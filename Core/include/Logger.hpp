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
  static Logger &get_instance();
  ~Logger() = default;

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
  Core::Logger::get_instance().info(format, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::get_instance().debug(format, std::forward<Args>(args)...);
}

template <typename... Args>
void trace(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::get_instance().trace(format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(fmt::format_string<Args...> format, Args &&...args) {
  Core::Logger::get_instance().error(format, std::forward<Args>(args)...);
}

#include "Logger.inl"
