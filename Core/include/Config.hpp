#pragma once

#include "Types.hpp"

namespace Core::Config {

#ifdef GPGPU_FRAME_COUNT
static constexpr u32 frame_count = GPGPU_FRAME_COUNT;
#else
static constexpr u32 frame_count = 3;
#endif

#ifdef GPGPU_THREAD_COUNT
static constexpr u32 thread_count = GPGPU_THREAD_COUNT;
#else
static constexpr u32 thread_count = 4;
#endif

} // namespace Core::Config
