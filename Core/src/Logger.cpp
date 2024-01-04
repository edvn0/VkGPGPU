#include "Logger.hpp"

#include <cctype>
#include <cstdlib>

namespace Core {

Logger::Logger() : current_level(get_log_level_from_environment()) {}

Logger &Logger::getInstance() {
  static Logger instance;
  return instance;
}

void Logger::set_level(LogLevel level) { current_level = level; }

LogLevel Logger::get_level() const { return current_level; }

auto to_lower(const std::string &str) {
  std::string lowerStr;
  for (char ch : str) {
    lowerStr += static_cast<char>(std::tolower(ch));
  }
  return lowerStr;
}

LogLevel Logger::get_log_level_from_environment() {
  const auto env_value = std::getenv("LOG_LEVEL");
  if (env_value != nullptr) {
    std::cout << "Log level: " << env_value << "\n";
    std::string logLevelStr = to_lower(env_value);
    if (logLevelStr == "trace" || logLevelStr == "t" || logLevelStr == "tr" ||
        logLevelStr == "tra")
      return LogLevel::Trace;
    if (logLevelStr == "debug" || logLevelStr == "d" || logLevelStr == "de" ||
        logLevelStr == "deb")
      return LogLevel::Debug;
    if (logLevelStr == "info" || logLevelStr == "i" || logLevelStr == "in" ||
        logLevelStr == "inf")
      return LogLevel::Info;
    if (logLevelStr == "error" || logLevelStr == "e" || logLevelStr == "er" ||
        logLevelStr == "err")
      return LogLevel::Error;
    if (logLevelStr == "none" || logLevelStr == "n" || logLevelStr == "no" ||
        logLevelStr == "non")
      return LogLevel::None;
  }
  return LogLevel::Info; // Default log level
}

} // namespace Core

// Implementation of the logger methods will be in Logger.inl