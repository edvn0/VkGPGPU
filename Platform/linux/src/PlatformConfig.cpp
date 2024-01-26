#include "PlatformConfig.hpp"

#include <array>
#include <string>
#include <unistd.h>

namespace Core::Platform {

auto get_system_name() -> std::string {
  std::array<char, 256> buffer{};
  if (gethostname(buffer.data(), buffer.size()) != 0) {
    return "default"; // Fallback name
  }
  return std::string(buffer.data());
}

} // namespace Core::Platform
