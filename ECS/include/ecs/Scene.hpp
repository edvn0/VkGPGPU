#pragma once

#include "ISceneObserver.hpp"
#include "Types.hpp"

#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace ECS {

class Entity;

class Scene {
public:
  auto create_entity(std::string_view) -> Entity;

private:
  std::string name{};
  entt::registry registry;
  std::vector<Core::Scope<ISceneObserver>> observers{};
};

} // namespace ECS
