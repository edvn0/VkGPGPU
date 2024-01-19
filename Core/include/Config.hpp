#pragma once

#include "Types.hpp"

namespace Core::Config {

#ifdef GPGPU_FRAME_COUNT
static constexpr u32 frame_count = GPGPU_FRAME_COUNT;
#else
static constexpr auto frame_count = 3;
#endif

} // namespace Core::Config
