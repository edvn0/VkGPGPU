#pragma once

#include <variant>

#include "ecs/messages/EntityEvents.hpp"
#include "ecs/messages/SceneEvents.hpp"

namespace ECS {

using Message =
    std::variant<Events::EntityAddedEvent, Events::EntityRemovedEvent,
                 Events::SceneDestroyedEvent>;

}
