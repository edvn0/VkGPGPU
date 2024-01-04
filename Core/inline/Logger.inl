#pragma once

#include <iostream>

namespace Core {

template <typename... Args>
void Logger::trace(fmt::format_string<Args...> format, Args &&...args) {
  if (current_level > LogLevel::Trace)
    return;
  std::cout << "[TRACE] " << fmt::format(format, std::forward<Args>(args)...)
            << std::endl;
}

template <typename... Args>
void Logger::debug(fmt::format_string<Args...> format, Args &&...args) {
  if (current_level > LogLevel::Debug)
    return;
  std::cout << "[DEBUG] " << fmt::format(format, std::forward<Args>(args)...)
            << std::endl;
}

template <typename... Args>
void Logger::info(fmt::format_string<Args...> format, Args &&...args) {
  if (current_level > LogLevel::Info)
    return;
  std::cout << "[INFO] " << fmt::format(format, std::forward<Args>(args)...)
            << std::endl;
}

template <typename... Args>
void Logger::error(fmt::format_string<Args...> format, Args &&...args) {
  // Error messages are always logged, regardless of the current log level
  std::cerr << "[ERROR] " << fmt::format(format, std::forward<Args>(args)...)
            << std::endl;
}

} // namespace Core
