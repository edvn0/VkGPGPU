#pragma once

#include "messages/Message.hpp"

namespace ECS {

class ISceneObserver {
public:
  virtual ~ISceneObserver() = default;

  virtual auto on_notify(const Message &) -> void = 0;
};

} // namespace ECS
