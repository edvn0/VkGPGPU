#pragma once

#include <entt/fwd.hpp>

#include "ecs/ISceneObserver.hpp"
#include "ecs/Scene.hpp"

namespace ECS {

class Entity : public ISceneObserver {
public:
  Entity(Scene *scene, std::string name);
  ~Entity() override;

  [[nodiscard]] auto get_id() const -> Core::u64;

  template <class T> [[nodiscard]] auto add_component(T &&component) -> T & {
    return scene->registry.emplace<T>(handle, std::forward<T>(component));
  }
  template <class T> [[nodiscard]] auto put_component(T &&component) -> T & {
    return scene->registry.emplace_or_replace<T>(handle,
                                                 std::forward<T>(component));
  }
  template <class T> [[nodiscard]] auto get_component() -> T & {
    return scene->registry.get<T>(handle);
  }
  template <class T> [[nodiscard]] auto has_component() -> bool {
    return scene->registry.any_of<T>(handle);
  }
  template <class... Ts> [[nodiscard]] auto all_of() -> bool {
    return scene->registry.all_of<Ts...>(handle);
  }

  auto on_notify(const Message &) -> void override;

private:
  Scene *scene;
  entt::entity handle;
  std::string name;
};

} // namespace ECS
