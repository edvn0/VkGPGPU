#pragma once

#include "Types.hpp"
#include <vulkan/vulkan.h>

namespace Core {

class Instance {
  using Ptr = const Instance *const;

public:
  [[nodiscard]] static auto get() -> Ptr;
  ~Instance();
  static void destroy() { static_instance.reset(); }

  [[nodiscard]] auto get_instance() const -> VkInstance { return instance; }

private:
  VkInstance instance{nullptr};

  static auto construct_instance() -> Scope<Instance>;
  auto construct_vulkan_instance() -> void;

  static inline Scope<Instance> static_instance{nullptr};
};

} // namespace Core