#pragma once

#include "Mesh.hpp"
#include "Types.hpp"

#include <string>

#include "ecs/UUID.hpp"

namespace ECS {

struct IdentityComponent {
  std::string name{"Empty"};
  Core::u64 id{0};

  explicit IdentityComponent(std::string name, Core::u64 identifier)
      : name(std::move(name)), id(identifier) {}
  explicit IdentityComponent(std::string name)
      : IdentityComponent(std::move(name), UUID::generate_uuid<64>()) {}

  IdentityComponent() = default;
};

struct TransformComponent {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  [[nodiscard]] auto compute() const -> glm::mat4;
};

struct TextureComponent {
  glm::vec4 colour{1.0F};
};

struct MeshComponent {
  Core::Ref<Core::Mesh> mesh;
  std::filesystem::path path;
};

struct CameraComponent {
  float field_of_view{glm::radians(90.0F)};
};

} // namespace ECS
