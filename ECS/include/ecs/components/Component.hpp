#pragma once

#include "AABB.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include "RenderingDefinitions.hpp"
#include "TypeUtility.hpp"
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
  auto update(const glm::vec3 &position, const glm::quat &rotation,
              const glm::vec3 &scale) -> void;

  [[nodiscard]] auto get_rotation_in_euler_angles() const -> glm::vec3 {
    return glm::eulerAngles(rotation);
  }

  auto set_rotation_as_euler_angles(const glm::vec3 &euler_angles) -> void {
    rotation = glm::quat(euler_angles);
  }

  static constexpr std::string_view component_name{"Transform"};
};

struct TextureComponent {
  glm::vec4 colour{1.0F};

  static constexpr std::string_view component_name{"Texture"};
};

struct MeshComponent {
  Core::Ref<Core::Mesh> mesh;
  std::filesystem::path path;
  bool draw_aabb{false};

  static constexpr std::string_view component_name{"Mesh"};
};

namespace BasicGeometry {
enum class Type : Core::u8 { Quad, Triangle, Circle, Sphere, Cube };
// Define structures for each geometry's parameters
struct QuadParameters {
  float width;
  float height;
};

struct TriangleParameters {
  float base;
  float height;
};

struct CircleParameters {
  float radius;
};

struct SphereParameters {
  float radius;
};

struct CubeParameters {
  float side_length;
};

// GeometryVariant to encapsulate all geometry types
using GeometryVariant =
    std::variant<QuadParameters, TriangleParameters, CircleParameters,
                 SphereParameters, CubeParameters>;

auto get_aabb_for_geometry(const ECS::BasicGeometry::GeometryVariant &geom,
                           const glm::mat4 &transform) -> Core::AABB;

} // namespace BasicGeometry
struct GeometryComponent {
  BasicGeometry::GeometryVariant parameters;
  static constexpr std::string_view component_name{"Geometry"};
};

struct CameraComponent {
  float field_of_view{glm::radians(90.0F)};
  Core::CameraType camera_type{Core::CameraType::Perspective};
  float near{0.1F};
  float far{10000.0F};

  static constexpr std::string_view component_name{"Camera"};
};

struct SunComponent {
  glm::vec4 colour;
  glm::vec4 specular_colour;
  glm::vec3 direction;
  Core::DepthParameters depth_params{};

  static constexpr std::string_view component_name{"Sun"};
};

struct PointLightComponent {
  glm::vec3 radiance = {1.0f, 1.0f, 1.0f};
  float intensity = 1.0f;
  float light_size = 0.5f;
  float min_radius = 1.f;
  float radius = 10.0f;
  bool casts_shadows = true;
  bool soft_shadows = true;
  float falloff = 1.0f;

  static constexpr std::string_view component_name{"PointLight"};
};

struct SpotLightComponent {
  glm::vec3 radiance{1.0f};
  float intensity = 1.0f;
  float range = 10.0f;
  float angle = 60.0f;
  float angle_attenuation = 5.0f;
  bool casts_shadows = false;
  bool soft_shadows = false;
  float falloff = 1.0f;

  static constexpr std::string_view component_name{"SpotLight"};
};

struct ParentComponent {
  Core::u64 parent; // Handle to the parent entity

  static constexpr std::string_view component_name{"Parent"};
};

struct ChildComponent {
  std::vector<Core::u64> children; // Handles to child entities

  static constexpr std::string_view component_name{"Child"};
};

template <typename... Ts> using ComponentList = Core::Typelist<Ts...>;

using EngineComponents =
    ComponentList<IdentityComponent, TransformComponent, TextureComponent,
                  MeshComponent, CameraComponent, SunComponent, ParentComponent,
                  ChildComponent, GeometryComponent, PointLightComponent,
                  SpotLightComponent>;
using UnremovableComponents =
    ComponentList<IdentityComponent, TransformComponent, ParentComponent,
                  ChildComponent>;

namespace Detail {
// Primary template, assume the type is not in the list
template <typename T, typename Typelist> struct IsInTypelist;

// Specialization for Typelist
template <typename T, typename... Types>
struct IsInTypelist<T, ComponentList<Types...>> {
  // Initial value is false
  static constexpr bool value = false;
};

// Partial specialization for when a match is found
template <typename T, typename... Types>
struct IsInTypelist<T, ComponentList<T, Types...>> {
  // Found T in the list, set value to true
  static constexpr bool value = true;
};

// Recursive case: If the first type doesn't match, strip the first type and try
// again
template <typename T, typename First, typename... Rest>
struct IsInTypelist<T, ComponentList<First, Rest...>> {
  static constexpr bool value = IsInTypelist<T, ComponentList<Rest...>>::value;
};

// Helper variable template for easier usage
template <typename T, typename Typelist>
inline constexpr bool IsInTypelist_v = IsInTypelist<T, Typelist>::value;
} // namespace Detail

template <typename T>
concept IsComponent = Detail::IsInTypelist_v<T, EngineComponents>;

} // namespace ECS
