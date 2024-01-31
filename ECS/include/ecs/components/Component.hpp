#pragma once

#include "Mesh.hpp"
#include "Types.hpp"

#include <string>

#include "ecs/UUID.hpp"

namespace ECS {

struct IdentityComponent {
  std::string name{"Empty"};
  Core::u64 id{0};

  explicit IdentityComponent(std::string name)
      : name(std::move(name)), id(UUID::generate_uuid<64>()) {}
};

struct TransformComponent {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  auto compute() const -> glm::mat4;
};

struct TextureComponent {
  glm::vec4 colour{1.0F};
};

struct MeshComponent {
  Core::Ref<Core::Mesh> mesh;
};

} // namespace ECS
