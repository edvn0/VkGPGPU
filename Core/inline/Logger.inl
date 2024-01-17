#pragma once

namespace Core {

#ifndef GPGPU_RELEASE
template <typename... Args>
void Logger::trace(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {
  if (current_level > LogLevel::Trace)
    return;
  log(fmt::format(format, std::forward<Args>(args)...), LogLevel::Trace);
}

template <typename... Args>
void Logger::debug(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {
  if (current_level > LogLevel::Debug)
    return;
  log(fmt::format(format, std::forward<Args>(args)...), LogLevel::Debug);
}

#else
template <typename... Args>
void Logger::trace(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {}

template <typename... Args>
void Logger::debug(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {}

#endif

template <typename... Args>
void Logger::info(fmt::format_string<Args...> format, Args &&...args) noexcept {
  if (current_level > LogLevel::Info)
    return;
  log(fmt::format(format, std::forward<Args>(args)...), LogLevel::Info);
}

template <typename... Args>
void Logger::error(fmt::format_string<Args...> format,
                   Args &&...args) noexcept {
  log(fmt::format(format, std::forward<Args>(args)...), LogLevel::Error);
}

} // namespace Core
