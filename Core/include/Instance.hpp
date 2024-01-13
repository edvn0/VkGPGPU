#pragma once

#include "Types.hpp"

#include <vulkan/vulkan.h>

namespace Core {

class Instance {
public:
  Instance();
  virtual ~Instance();

  [[nodiscard]] auto get_instance() const -> VkInstance { return instance; }

  static auto construct() -> Scope<Instance>;

private:
  VkInstance instance{nullptr};

  auto construct_vulkan_instance() -> void;
};

} // namespace Core
