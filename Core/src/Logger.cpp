#include "pch/vkgpgpu_pch.hpp"

#include "Logger.hpp"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

namespace Core {

Logger::Logger() : current_level(get_log_level_from_environment()) {
  worker = std::jthread(
      [this](const std::stop_token &stop_token) { process_queue(stop_token); });
}

Logger &Logger::get_instance() {
  static Logger instance;
  return instance;
}

void Logger::stop() { get_instance().stop_all(); }

Logger::~Logger() { stop_all(); }

void Logger::stop_all() {
  // If the worker thread is already stopped, return
  if (exitFlag)
    return;

  exitFlag = true;
  cv.notify_one();
  worker.join();
}

void Logger::log(std::string &&message, LogLevel level) {
  std::lock_guard lock(queue_mutex);
  log_queue.emplace(std::move(message), level);
  cv.notify_one();
}

void Logger::process_queue(const std::stop_token &stop_token) {
  while (!stop_token.stop_requested()) {
    std::unique_lock lock(queue_mutex);
    cv.wait(lock, [this] { return !log_queue.empty() || exitFlag; });

    if (exitFlag && log_queue.empty())
      break;

    while (!log_queue.empty()) {
      auto &&log_message = log_queue.front();
      process_single(log_message);
      log_queue.pop();
    }
  }
}

namespace AnsiColor {
using namespace std::string_view_literals;
static constexpr auto Reset = "\033[0m"sv;
static constexpr auto Red = "\033[31m"sv;    // Error
static constexpr auto Green = "\033[32m"sv;  // Info
static constexpr auto Yellow = "\033[33m"sv; // Debug
static constexpr auto Blue = "\033[34m"sv;   // Trace
} // namespace AnsiColor

void Logger::process_single(const BackgroundLogMessage &message) {
  switch (message.level) {
    using enum Core::LogLevel;
  case Trace:
    std::cout << AnsiColor::Blue << "[TRACE] " << message.message
              << AnsiColor::Reset << std::endl;
    break;
  case Debug:
    std::cout << AnsiColor::Yellow << "[DEBUG] " << message.message
              << AnsiColor::Reset << std::endl;
    break;
  case Info:
    std::cout << AnsiColor::Green << "[INFO] " << message.message
              << AnsiColor::Reset << std::endl;
    break;
  case Error:
    std::cout << AnsiColor::Red << "[ERROR] " << message.message
              << AnsiColor::Reset << std::endl;
    break;
  case None:
    break;
  }
}

void Logger::set_level(LogLevel level) { current_level = level; }

LogLevel Logger::get_level() const { return current_level; }

auto to_lower(const std::string &str) {
  std::string lower_str;
  lower_str.reserve(str.size());
  for (const char ch : str) {
    lower_str += static_cast<char>(std::tolower(ch));
  }
  return lower_str;
}

LogLevel Logger::get_log_level_from_environment() {
  if (const auto env_value = std::getenv("LOG_LEVEL"); env_value != nullptr) {
    std::cout << "Log level: " << env_value << "\n";
    const std::string log_level = to_lower(env_value);
    if (log_level == "trace" || log_level == "t" || log_level == "tr" ||
        log_level == "tra")
      return LogLevel::Trace;
    if (log_level == "debug" || log_level == "d" || log_level == "de" ||
        log_level == "deb")
      return LogLevel::Debug;
    if (log_level == "info" || log_level == "i" || log_level == "in" ||
        log_level == "inf")
      return LogLevel::Info;
    if (log_level == "error" || log_level == "e" || log_level == "er" ||
        log_level == "err")
      return LogLevel::Error;
    if (log_level == "none" || log_level == "n" || log_level == "no" ||
        log_level == "non")
      return LogLevel::None;
  }
  return LogLevel::Info; // Default log level
}

} // namespace Core
