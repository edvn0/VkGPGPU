#include "Logger.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include <cctype>
#include <cstdlib>

namespace Core {

Logger::Logger() : current_level(get_log_level_from_environment()) {}

Logger &Logger::get_instance() {
  static Logger instance;
  return instance;
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
