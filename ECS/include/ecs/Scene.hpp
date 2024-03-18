#pragma once

#include "Camera.hpp"
#include "ISceneObserver.hpp"
#include "Math.hpp"
#include "ThreadPool.hpp"
#include "Types.hpp"

#include <entt/entt.hpp>
#include <fmt/core.h>
#include <string>
#include <vector>

#include "core/Forward.hpp"

namespace ECS {

class Entity;

struct DirectionalLight {
  glm::vec3 direction = {0.0f, 0.0f, 0.0f};
  glm::vec3 radiance = {0.0f, 0.0f, 0.0f};
  float intensity = 0.0f;
  float shadow_amount = 1.0f;
  bool cast_shadows = true;
};

struct PointLight {
  glm::vec3 position = {0.0f, 0.0f, 0.0f};
  float intensity = 0.0f;
  glm::vec3 radiance = {0.0f, 0.0f, 0.0f};
  float min_radius = 0.001f;
  float radius = 25.0f;
  float falloff = 1.f;
  float source_size = 0.1f;
  bool casts_shadows = true;
  std::array<char, 3> Padding{0, 0, 0};
};

struct SpotLight {
  glm::vec3 position = {0.0f, 0.0f, 0.0f};
  float intensity = 0.0f;
  glm::vec3 direction = {0.0f, 0.0f, 0.0f};
  float angle_attenuation = 0.0f;
  glm::vec3 radiance = {0.0f, 0.0f, 0.0f};
  float range = 0.1f;
  float angle = 0.0f;
  float falloff = 1.0f;
  bool soft_shadows = true;
  std::array<char, 3> padding0{0, 0, 0};
  bool casts_shadows = true;
  std::array<char, 3> padding1{0, 0, 0};
};

struct LightEnvironment {
  std::array<DirectionalLight, 4> directional_lights;
  std::vector<PointLight> point_light_buffer;
  std::vector<SpotLight> spot_light_buffer;
  [[nodiscard]] auto get_point_light_buffer_size_bytes() const -> Core::u32 {
    return static_cast<Core::u32>(point_light_buffer.size() *
                                  sizeof(PointLight));
  }
  [[nodiscard]] auto get_spot_light_buffer_size_bytes() const -> Core::u32 {
    return static_cast<Core::u32>(spot_light_buffer.size() * sizeof(SpotLight));
  }
};

class Scene {
public:
  explicit Scene(std::string_view scene_name);
  ~Scene();
  auto create_entity(std::string_view, bool add_observer = true) -> Entity;

  template <typename... Args>
  auto create_entity(fmt::format_string<Args...> fmt, Args &&...args) {
    return create_entity(fmt::format(fmt, std::forward<Args>(args)...), true);
  }
  [[nodiscard]] auto delete_entity(Core::u64 identifier) -> bool;

  // Lifetime events
  auto on_create(const Core::Device &, const Core::Window &,
                 const Core::Swapchain &) -> void;
  auto on_destroy() -> void;
  auto on_update(Core::SceneRenderer &, Core::floating) -> void;

  auto on_update_runtime(Core::floating ts) -> void;
  auto on_update_editor(Core::floating ts) -> void;
  auto on_runtime_start() -> void;
  auto on_runtime_stop() -> void;

  auto on_simulation_start() -> void;
  auto on_simulation_stop() -> void;

  auto on_render_runtime(Core::SceneRenderer &, Core::floating ts) -> void;
  auto on_render_editor(Core::SceneRenderer &, Core::floating ts,
                        const Core::EditorCamera &) -> void;
  auto on_render_simulation(Core::SceneRenderer &, Core::floating ts,
                            const Core::EditorCamera &) -> void;

  auto on_resize(const Core::Extent<Core::u32> &) -> void;

  auto on_render(Core::SceneRenderer &, Core::floating ts,
                 const Core::Math::Mat4 &projection_matrix,
                 const Core::Math::Mat4 &view_matrix) -> void;

  [[nodiscard]] auto get_scene_name() const -> std::string_view { return name; }
  [[nodiscard]] auto get_registry() -> entt::registry & { return registry; }
  [[nodiscard]] auto get_registry() const -> const entt::registry & {
    return registry;
  }
  [[nodiscard]] auto get_entity(entt::entity) -> std::optional<Entity>;
  [[nodiscard]] auto get_entity(Core::u64) -> std::optional<Entity>;

  auto clear() -> void { registry.clear(); }
  auto save(std::string_view = "") -> void;
  auto sort(auto &&func) -> void {
    registry.sort(std::forward<decltype(func)>(func));
  }
  auto sort() -> void;
  void set_scene_name(const std::string_view scene_name) { name = scene_name; }
  void initialise_device_dependent_objects(const Core::Device &device);

  auto copy_to(Scene &other) -> void;

  template <class... Args> auto view() { return registry.view<Args...>(); }

  auto get_light_environment() const { return light_environment; }

private:
  std::string name{};
  entt::registry registry;
  std::vector<ISceneObserver *> observers{};

  LightEnvironment light_environment;
  bool is_playing{false};
  bool should_simulate{false};

  Core::ThreadPool pool;
  std::queue<std::future<void>> futures;

  friend class Entity;
  friend class ImmutableEntity;
};

} // namespace ECS
