#pragma once

#include "Device.hpp"

#include <vulkan/vulkan.h>

namespace Core::Destructors {

template <class T>
inline auto destroy(const Device &device, const T &to_destroy) = delete;

}

#include "Destructors.inl"
