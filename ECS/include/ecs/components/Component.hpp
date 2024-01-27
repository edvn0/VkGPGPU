#pragma once

#include "Types.hpp"

#include <string>

#include "ecs/UUID.hpp"

namespace ECS {

struct IdentityComponent {
  std::string name{"Empty"};
  Core::u64 id{0};

  explicit IdentityComponent(std::string name)
      : name(std::move(name)), id(UUID::generate_uuid<64>()) {}

  auto operator==(const IdentityComponent &other) const -> bool {
    return id == other.id && name == other.name;
  }
  auto operator<=>(const IdentityComponent &) const = default;
};

} // namespace ECS
