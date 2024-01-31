#pragma once

#include "Types.hpp"

namespace ECS::Events {

struct EntityAddedEvent {
  Core::u64 id;
};

struct EntityRemovedEvent {
  Core::u64 id;
};

} // namespace ECS::Events
