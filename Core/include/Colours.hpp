#pragma once

#include "Math.hpp"

namespace Core::Colours {

using Colour = Math::Vec4;

static constexpr auto black = Colour(0.0f, 0.0f, 0.0f, 1.0f);
static constexpr auto white = Colour(1.0f, 1.0f, 1.0f, 1.0f);
static constexpr auto red = Colour(1.0f, 0.0f, 0.0f, 1.0f);
static constexpr auto green = Colour(0.0f, 1.0f, 0.0f, 1.0f);
static constexpr auto blue = Colour(0.0f, 0.0f, 1.0f, 1.0f);
static constexpr auto yellow = Colour(1.0f, 1.0f, 0.0f, 1.0f);
static constexpr auto cyan = Colour(0.0f, 1.0f, 1.0f, 1.0f);
static constexpr auto magenta = Colour(1.0f, 0.0f, 1.0f, 1.0f);

} // namespace Core::Colours
