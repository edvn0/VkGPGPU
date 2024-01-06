#pragma once

namespace Core::Config {

#ifdef GPGPU_FRAME_COUNT
static constexpr auto frame_count = GPGPU_FRAME_COUNT;
#else
static constexpr auto frame_count = 3;
#endif

}