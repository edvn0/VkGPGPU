#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Entity.hpp"

#include "Containers.hpp"
#include "Logger.hpp"

#include <entt/entt.hpp>

#include "ecs/Scene.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

Entity::Entity(Scene *scene, std::string_view input_name)
    : scene(scene), handle(scene->registry.create()), name(input_name) {
  (void)add_component<IdentityComponent>(name);
  (void)add_component<TransformComponent>();
}

Entity::~Entity() = default;

auto Entity::get_id() const -> Core::u64 {
  return scene->registry.get<IdentityComponent>(handle).id;
}

auto Entity::on_notify(const Message &message) -> void {
  std::visit(
      [this](auto &&arg) {
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

auto Entity::get_transform() const -> TransformComponent & {
  return get_component<TransformComponent>();
}
} // namespace ECS
