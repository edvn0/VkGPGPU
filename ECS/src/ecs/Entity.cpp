#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Entity.hpp"

#include "Logger.hpp"

#include <entt/entt.hpp>

#include "ecs/Scene.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

Entity::Entity(Scene *scene, std::string input_name)
    : scene(scene), handle(scene->registry.create()),
      name(std::move(input_name)) {
  scene->registry.emplace<IdentityComponent>(handle, name);
}

Entity::~Entity() {
  for (auto observer : scene->observers) {
    observer->on_notify(Events::EntityRemovedEvent{
        .id = get_id(),
    });
  }
  scene->registry.destroy(handle);
  scene->observers.erase(
      std::remove_if(scene->observers.begin(), scene->observers.end(),
                     [this](auto observer) { return observer == this; }),
      scene->observers.end());
}

auto Entity::get_id() const -> Core::u64 {
  return scene->registry.get<IdentityComponent>(handle).id;
}

auto Entity::on_notify(const Message &message) -> void {
  std::visit(
      [this]<typename Variant>(Variant &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Events::EntityAddedEvent>) {
          info("Entity {} with id: {} added to scene {}", name, arg.id,
               scene->name);
        } else if constexpr (std::is_same_v<T, Events::EntityRemovedEvent>) {
          info("Entity {} with id: {} removed from scene {}", name, arg.id,
               scene->name);
        } else if constexpr (std::is_same_v<T, Events::SceneDestroyedEvent>) {
          info("Scene {} destroyed", scene->name);
        }
      },
      message);
}

} // namespace ECS
