#pragma once

#include <atomic>
#include <condition_variable>
#include <fmt/core.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Core {

enum class LogLevel {
  Trace,
  Debug,
  Info,
  Error,
  None // To disable logging
};

struct BackgroundLogMessage {
  std::string message;
  LogLevel level{LogLevel::None};
};

class Logger {
public:
  static Logger &get_instance();
  static void stop();
  ~Logger();

  void set_level(LogLevel level);
  auto get_level() const -> LogLevel;

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  template <typename... Args>
  void info(fmt::format_string<Args...> format, Args &&...args) noexcept;

  template <typename... Args>
  void debug(fmt::format_string<Args...> format, Args &&...args) noexcept;

  template <typename... Args>
  void trace(fmt::format_string<Args...> format, Args &&...args) noexcept;

  template <typename... Args>
  void error(fmt::format_string<Args...> format, Args &&...args) noexcept;

private:
  Logger();

  void stop_all();
  void log(std::string &&message, LogLevel level);
  void process_queue(const std::stop_token &);

  LogLevel current_level{LogLevel::None};
  static void process_single(const BackgroundLogMessage &message);
  static LogLevel get_log_level_from_environment();

  std::queue<BackgroundLogMessage> log_queue;
  std::mutex queue_mutex;
  std::condition_variable cv;
  std::jthread worker;
  std::atomic_bool exitFlag;
};

} // namespace Core

template <typename... Args>
void info(fmt::format_string<Args...> format, Args &&...args) noexcept {
  Core::Logger::get_instance().info(format, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(fmt::format_string<Args...> format, Args &&...args) noexcept {
  Core::Logger::get_instance().debug(format, std::forward<Args>(args)...);
}

template <typename... Args>
void trace(fmt::format_string<Args...> format, Args &&...args) noexcept {
  Core::Logger::get_instance().trace(format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(fmt::format_string<Args...> format, Args &&...args) noexcept {
  Core::Logger::get_instance().error(format, std::forward<Args>(args)...);
}

#include "Logger.inl"
