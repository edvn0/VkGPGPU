#pragma once

#include <entt/fwd.hpp>

#include "ecs/ISceneObserver.hpp"
#include "ecs/Scene.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

class Entity : public ISceneObserver {
public:
  Entity(Scene *scene, std::string_view name);
  Entity(Scene *scene, entt::entity, std::string_view name);
  ~Entity() override;

  [[nodiscard]] auto get_id() const -> Core::u64;
  [[nodiscard]] auto get_handle() const { return handle; }
  [[nodiscard]] auto get_name() const -> const std::string &;

  template <class T> auto add_component(T &&component) -> decltype(auto) {
    return scene->registry.emplace<T>(handle, std::forward<T>(component));
  }
  template <class T> auto add_component() -> decltype(auto) {
    if (scene->registry.any_of<T>(handle)) {
      return scene->registry.get<T>(handle);
    }

    return scene->registry.emplace<T>(handle, T{});
  }

  template <class T, class... Args>
  auto add_component(Args &&...args) -> decltype(auto) {
    if (scene->registry.any_of<T>(handle)) {
      return scene->registry.get<T>(handle);
    }

    return scene->registry.emplace<T>(handle, std::forward<Args>(args)...);
  }
  template <class T> auto put_component(T &&component) -> decltype(auto) {
    return scene->registry.emplace_or_replace<T>(handle,
                                                 std::forward<T>(component));
  }
  template <class T> [[nodiscard]] auto get_component() -> decltype(auto) {
    return scene->registry.get<T>(handle);
  }
  template <class T>
  [[nodiscard]] auto get_component() const -> decltype(auto) {
    return scene->registry.get<T>(handle);
  }
  template <class T> [[nodiscard]] auto has_component() const -> bool {
    return scene->registry.any_of<T>(handle);
  }
  template <class... Ts> [[nodiscard]] auto all_of() const -> bool {
    return scene->registry.all_of<Ts...>(handle);
  }
  template <class... Ts> [[nodiscard]] auto any_of() const -> bool {
    return scene->registry.any_of<Ts...>(handle);
  }
  template <class T> auto remove_component() const -> void {
    scene->registry.remove<T>(handle);
  }

  auto on_notify(const Message &) -> void override;

  [[nodiscard]] auto get_transform() const -> TransformComponent &;

private:
  Scene *scene;
  entt::entity handle;
};

} // namespace ECS
