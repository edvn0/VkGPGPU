#pragma once

namespace Core {

template <typename... Ts> struct Typelist {};

// Base template (unused, but needed for specialization)
template <typename T> struct TypelistToTuple {};

// Specialization that converts Typelist to std::tuple
template <typename... Ts> struct TypelistToTuple<Typelist<Ts...>> {
  using type = std::tuple<Ts...>;
};

} // namespace Core
