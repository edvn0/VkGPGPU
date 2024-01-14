#pragma once

#include <iostream>

namespace Core {

namespace AnsiColor {
static constexpr const char *Reset = "\033[0m";
static constexpr const char *Red = "\033[31m";    // Error
static constexpr const char *Green = "\033[32m";  // Info
static constexpr const char *Yellow = "\033[33m"; // Debug
static constexpr const char *Blue = "\033[34m";   // Trace
} // namespace AnsiColor

#ifdef GPGPU_DEBUG
template <typename... Args>
void Logger::trace(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {
  if (current_level > LogLevel::Trace)
    return;
  std::cout << AnsiColor::Blue << "[TRACE] "
            << fmt::format(format, std::forward<Args>(args)...)
            << AnsiColor::Reset << "\n";
}

template <typename... Args>
void Logger::debug(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {
  if (current_level > LogLevel::Debug)
    return;
  std::cout << AnsiColor::Yellow << "[DEBUG] "
            << fmt::format(format, std::forward<Args>(args)...)
            << AnsiColor::Reset << "\n";
}

template <typename... Args>
void Logger::info(fmt::format_string<Args...> format, Args &&...args) noexcept {
  if (current_level > LogLevel::Info)
    return;
  std::cout << AnsiColor::Green << "[INFO] "
            << fmt::format(format, std::forward<Args>(args)...)
            << AnsiColor::Reset << "\n";
}

template <typename... Args>
void Logger::error(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {
  std::cerr << AnsiColor::Red << "[ERROR] "
            << fmt::format(format, std::forward<Args>(args)...)
            << AnsiColor::Reset << "\n";
}
#else
template <typename... Args>
void Logger::trace(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {}

template <typename... Args>
void Logger::debug(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {}

template <typename... Args>
void Logger::info(fmt::format_string<Args...> format, Args &&...args) noexcept {
}

template <typename... Args>
void Logger::error(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {}
#endif

} // namespace Core
