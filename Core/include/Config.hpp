#pragma once

#include "Types.hpp"

namespace Core::Config {

enum class Precision : u8 { Low, Medium, High };

#ifdef GPGPU_PRECISION
static constexpr Precision precision = Precision::GPGPU_PRECISION;
#else
static constexpr Precision precision = Precision::High;
#endif

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

#ifdef GPGPU_TRANSFORM_BUFFER_SIZE
static constexpr u32 transform_buffer_size = GPGPU_TRANSFORM_BUFFER_SIZE;
#else
static constexpr u32 transform_buffer_size = 1000;
#endif

} // namespace Core::Config
