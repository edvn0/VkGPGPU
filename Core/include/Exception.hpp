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

  [[nodiscard]] auto what() const noexcept -> const char * override {
    return message.c_str();
  }

private:
  std::string message;
};

class NotFoundException : public BaseException {
public:
  using BaseException::BaseException;
};

class UnableToOpenException : public BaseException {
public:
  using BaseException::BaseException;
};

} // namespace Core
