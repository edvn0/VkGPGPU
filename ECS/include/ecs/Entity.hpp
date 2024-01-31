#pragma once

#include <entt/fwd.hpp>

#include "ecs/ISceneObserver.hpp"
#include "ecs/Scene.hpp"
#include "ecs/components/Component.hpp"

namespace ECS {

class Entity : public ISceneObserver {
public:
  Entity(Scene *scene, std::string_view name);
  ~Entity() override;

  [[nodiscard]] auto get_id() const -> Core::u64;

  template <class T>
  [[nodiscard]] auto add_component(T &&component) -> decltype(auto) {
    return scene->registry.emplace<T>(handle, std::forward<T>(component));
  }

  template <class T, class... Args>
  [[nodiscard]] auto add_or_get_component(Args &&...args) -> decltype(auto) {
    return scene->registry.emplace_or_replace<T>(handle,
                                                 std::forward<Args>(args)...);
  }
  template <class T, class... Args>
  [[nodiscard]] auto add_component(Args &&...args) -> decltype(auto) {
    if (scene->registry.any_of<T>(handle)) {
      return scene->registry.get<T>(handle);
    }

    return scene->registry.emplace<T>(handle, std::forward<Args>(args)...);
  }
  template <class T>
  [[nodiscard]] auto put_component(T &&component) -> decltype(auto) {
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
  template <class T> [[nodiscard]] auto has_component() -> bool {
    return scene->registry.any_of<T>(handle);
  }
  template <class... Ts> [[nodiscard]] auto all_of() -> bool {
    return scene->registry.all_of<Ts...>(handle);
  }

  auto on_notify(const Message &) -> void override;

  [[nodiscard]] auto get_transform() const -> TransformComponent &;

private:
  Scene *scene;
  entt::entity handle;
  std::string name;
};

} // namespace ECS
