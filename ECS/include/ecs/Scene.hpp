#pragma once

#include "ISceneObserver.hpp"
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

  // Lifetime events
  auto on_create() -> void;
  auto on_destroy() -> void;
  auto on_update(Core::floating) -> void;
  auto on_interface(Core::InterfaceSystem &) -> void;
  auto on_resize(const Core::Extent<Core::u32> &) -> void;

private:
  std::string name{};
  entt::registry registry;
  std::vector<ISceneObserver *> observers{};

  friend class Entity;
};

} // namespace ECS
