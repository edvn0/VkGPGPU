#include "pch/vkgpgpu_pch.hpp"

#include "ecs/components/Component.hpp"

#include <glm/gtx/quaternion.hpp>

namespace ECS {

auto TransformComponent::compute() const -> glm::mat4 {
  return glm::translate(glm::mat4(1.0F), position) * glm::toMat4(rotation) *
         glm::scale(glm::mat4(1.0F), scale);
}

auto TransformComponent::update(const glm::vec3 &pos, const glm::quat &rot,
                                const glm::vec3 &scl) -> void {
  position = pos;
  rotation = rot;
  scale = scl;
}

} // namespace ECS
