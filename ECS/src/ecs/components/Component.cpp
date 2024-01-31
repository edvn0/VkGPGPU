#include "pch/vkgpgpu_pch.hpp"

#include "ecs/components/Component.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace ECS {

auto TransformComponent::compute() const -> glm::mat4 {
  return glm::translate(glm::mat4(1.0F), position) * glm::toMat4(rotation) *
         glm::scale(glm::mat4(1.0F), scale);
}

} // namespace ECS
