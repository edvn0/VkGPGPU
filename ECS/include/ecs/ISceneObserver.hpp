#pragma once

namespace ECS {

enum class SceneEvent {
  Closed,
  Deleted,
};

class ISceneObserver {
public:
  virtual ~ISceneObserver() = default;

  virtual auto on_notify(SceneEvent) -> void = 0;
};

} // namespace ECS
