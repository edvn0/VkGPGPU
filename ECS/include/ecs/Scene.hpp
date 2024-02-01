#pragma once

#include "ISceneObserver.hpp"
#include "Math.hpp"
#include "Types.hpp"

#include <entt/entt.hpp>
#include <string>
#include <vector>

#include "core/Forward.hpp"

namespace ECS {

class Entity;

class Scene {
public:
  explicit Scene(std::string_view scene_name);
  ~Scene();
  auto create_entity(std::string_view, bool add_observer = true) -> Entity;
  auto delete_entity(Core::u64 identifier) -> void;

  // Lifetime events
  auto on_create(const Core::Device &, const Core::Window &,
                 const Core::Swapchain &) -> void;
  auto temp_on_create(const Core::Device &, const Core::Window &,
                      const Core::Swapchain &) -> void;
  auto on_destroy() -> void;
  auto on_update(Core::SceneRenderer &, Core::floating) -> void;
  auto on_interface(Core::InterfaceSystem &) -> void;
  auto on_resize(const Core::Extent<Core::u32> &) -> void;

  auto on_render(Core::SceneRenderer &, Core::floating ts,
                 const Core::Math::Mat4 &projection_matrix,
                 const Core::Math::Mat4 &view_matrix) -> void;

  [[nodiscard]] auto get_scene_name() const -> std::string_view { return name; }
  [[nodiscard]] auto get_registry() -> entt::registry & { return registry; }
  [[nodiscard]] auto get_registry() const -> const entt::registry & {
    return registry;
  }

  auto clear() -> void { registry.clear(); }
  void set_scene_name(const std::string_view scene_name) { name = scene_name; }

private:
  std::string name{};
  entt::registry registry;
  std::vector<ISceneObserver *> observers{};

  friend class Entity;
};

} // namespace ECS
