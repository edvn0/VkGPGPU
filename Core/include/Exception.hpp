#pragma once

#include "Logger.hpp"

#include <exception>
#include <string>

namespace Core {

class BaseException : public std::exception {
public:
  explicit BaseException(const std::string &input) : message(input) {
    debug("Exception: {}", input);
  }

  auto what() const noexcept -> const char * override {
    return message.c_str();
  }

private:
  std::string message;
};

} // namespace Core
