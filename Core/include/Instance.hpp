#pragma once

#include "Types.hpp"

#include <vulkan/vulkan.h>

namespace Core {

class Instance {
public:
  explicit Instance(bool headless);
  virtual ~Instance();

  [[nodiscard]] auto get_instance() const -> VkInstance { return instance; }

  static auto construct() -> Scope<Instance>;
  static auto construct_headless() -> Scope<Instance>;

private:
  VkInstance instance{nullptr};

  auto construct_vulkan_instance(bool headless) -> void;
};

} // namespace Core
