#include "pch/vkgpgpu_pch.hpp"

#include "ecs/Entity.hpp"

#include "Containers.hpp"
#include "Logger.hpp"

#include <entt/entt.hpp>
#include <vector>

#include "ecs/Scene.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

Entity::Entity(Scene *scene, std::string_view input_name)
    : Entity(scene, scene->registry.create(), input_name) {}

Entity::Entity(Scene *scene, entt::entity entity_handle, std::string_view name)
    : scene(scene) {
  handle = entity_handle;
  add_component<IdentityComponent>(std::string{name});
  add_component<TransformComponent>();
}

Entity::Entity(Scene *scene, entt::entity entity_handle) : scene(scene) {
  handle = entity_handle;
}

Entity::~Entity() = default;

auto Entity::get_id() const -> Core::u64 {
  return scene->registry.get<IdentityComponent>(handle).id;
}

auto Entity::get_name() const -> const std::string & {
  return get_component<IdentityComponent>().name;
}

auto Entity::on_notify(const Message &message) -> void {
  std::visit(
      [this](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Events::EntityAddedEvent>) {
          info("Entity {} with id: {} added to scene {}", get_name(), arg.id,
               scene->name);
        } else if constexpr (std::is_same_v<T, Events::EntityRemovedEvent>) {
          info("Entity {} with id: {} removed from scene {}", get_name(),
               arg.id, scene->name);
        } else if constexpr (std::is_same_v<T, Events::SceneDestroyedEvent>) {
          info("Scene {} destroyed", scene->name);
        }
      },
      message);
}

auto Entity::get_transform() const -> TransformComponent & {
  return get_component<TransformComponent>();
}

void Entity::set_parent(const Entity &parent_entity) {
  auto parent = parent_entity.get_id(); // Assuming get_id returns Core::u64
  auto &parentComponent = add_component<ParentComponent>();
  parentComponent.parent = parent;

  // Add this entity as a child of the parent
  auto &childrenComponent =
      parent_entity.scene->registry.get_or_emplace<ChildComponent>(
          parent_entity.handle);
  childrenComponent.children.push_back(this->get_id());

  // Optionally, ensure no duplicates
  auto &childrenIDs = childrenComponent.children;
  if (std::ranges::find(childrenIDs, this->get_id()) == childrenIDs.end()) {
    childrenIDs.push_back(this->get_id());
  }
}

// Get parent entity, if it exists
std::optional<Entity> Entity::get_parent() const {
  if (scene->registry.all_of<ParentComponent>(handle)) {
    auto parent = scene->registry.get<ParentComponent>(handle).parent;
    // Find the entity by parent
    for (auto entity : scene->registry.view<IdentityComponent>()) {
      if (scene->registry.get<IdentityComponent>(entity).id == parent) {
        return Entity(scene, entity);
      }
    }
  }
  return std::nullopt; // No parent found
}

// Get children entities
std::vector<Entity> Entity::get_children() const {
  std::vector<Entity> childrenEntities;
  if (!scene->registry.all_of<ChildComponent>(handle))
    return childrenEntities;

  auto &children_identifiers =
      scene->registry.get<ChildComponent>(handle).children;

  for (auto child_identifer : children_identifiers) {
    // Find each child entity by its ID
    for (auto entity : scene->registry.view<IdentityComponent>()) {
      if (scene->registry.get<IdentityComponent>(entity).id ==
          child_identifer) {
        childrenEntities.emplace_back(scene, entity);
        break; // Found the child, move to next ID
      }
    }
  }
  return childrenEntities;
}

auto Entity::remove_all_components() const -> void {
  static constexpr auto delete_all = []<class... Ts>(ComponentList<Ts...> comp,
                                                     auto &reg, auto &entity) {
    (reg.template remove<Ts>(entity), ...);
  };

  delete_all(EngineComponents{}, scene->registry, handle);
  delete_all(UnremovableComponents{}, scene->registry, handle);
}

} // namespace ECS
