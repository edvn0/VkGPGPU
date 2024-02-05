#pragma once

#include "Types.hpp"

namespace ECS::Events {

static constexpr auto invalid_entity_id = ~Core::u64{};

struct EntityAddedEvent {
  Core::u64 id;
};

struct EntityRemovedEvent {
  Core::u64 id;
};

struct SelectedEntityUpdateEvent {
  Core::u64 id;

  static auto empty() -> SelectedEntityUpdateEvent {
    return {
        .id = invalid_entity_id,
    };
  }
};

} // namespace ECS::Events
