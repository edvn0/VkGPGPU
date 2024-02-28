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

#ifdef GPGPU_SHADOW_MAP_SIZE
static constexpr u32 shadow_map_size = GPGPU_SHADOW_MAP_SIZE;
#else
static constexpr u32 shadow_map_size = 4096;
#endif

#ifdef GPGPU_ENVIRONMENT_MAP_SIZE
static constexpr u32 environment_map_size = GPGPU_ENVIRONMENT_MAP_SIZE;
#else
static constexpr u32 environment_map_size = 512;
#endif

#ifdef GPGPU_USE_PIPELINE_CACHE
static constexpr bool use_pipeline_cache = GPGPU_USE_PIPELINE_CACHE;
#else
static constexpr bool use_pipeline_cache = true;
#endif

#ifdef GPGPU_ENABLE_RAYTRACING
static constexpr bool enable_ray_tracing = GPGPU_ENABLE_RAYTRACING;
#else
static constexpr bool enable_ray_tracing = false;
#endif

} // namespace Core::Config
