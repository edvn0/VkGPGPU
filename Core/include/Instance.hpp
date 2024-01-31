#pragma once

#include "Types.hpp"

#include <vulkan/vulkan.h>

namespace Core {

class Instance {
public:
  virtual ~Instance();

  [[nodiscard]] auto get_instance() const -> VkInstance { return instance; }

  static auto construct(bool) -> Scope<Instance>;

protected:
  explicit Instance(bool);

private:
  VkInstance instance{nullptr};
  VkDebugUtilsMessengerEXT debug_messenger{nullptr};

  auto construct_vulkan_instance(bool) -> void;
  auto setup_debug_messenger() -> void;
  bool enable_validation_layers{false};
};

} // namespace Core
