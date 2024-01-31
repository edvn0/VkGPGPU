#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Scene.hpp"

#include "Logger.hpp"

#include <entt/entt.hpp>

#include "ecs/Entity.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

Scene::Scene(std::string_view scene_name) : name{scene_name} {}

Scene::~Scene() {
  for (auto observer : observers) {
    observer->on_notify(Events::SceneDestroyedEvent{});
  }

  observers.clear();
}

auto Scene::create_entity(const std::string_view entity_name, bool add_observer)
    -> Entity {
  Entity entity{this, std::string{entity_name}};
  for (auto observer : observers) {
    observer->on_notify(Events::EntityAddedEvent{
        .id = entity.get_id(),
    });
  }

  if (add_observer) {
    observers.push_back(&entity);
  }
  return entity;
}

auto Scene::on_update(Core::floating dt) -> void {}

} // namespace ECS
