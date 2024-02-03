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

  static constexpr std::string_view component_name{"Identity"};
};

struct TransformComponent {
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};

  [[nodiscard]] auto compute() const -> glm::mat4;

  static constexpr std::string_view component_name{"Transform"};
};

struct TextureComponent {
  glm::vec4 colour{1.0F};

  static constexpr std::string_view component_name{"Texture"};
};

struct MeshComponent {
  Core::Ref<Core::Mesh> mesh;
  std::filesystem::path path;

  static constexpr std::string_view component_name{"Mesh"};
};

struct CameraComponent {
  float field_of_view{glm::radians(90.0F)};

  static constexpr std::string_view component_name{"Camera"};
};

template <typename...> struct ComponentList {};

using EngineComponents =
    ComponentList<IdentityComponent, TransformComponent, TextureComponent,
                  MeshComponent, CameraComponent>;
using UnremovableComponents =
    ComponentList<IdentityComponent, TransformComponent>;

} // namespace ECS
