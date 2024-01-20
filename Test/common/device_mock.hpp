#pragma once

#include "Device.hpp"
#include "Instance.hpp"
#include "Window.hpp"

class MockDevice : public Core::Device {
public:
  MockDevice(const Core::Instance &instance, const Core::Window &window)
      : Core::Device(instance, window) {}
  ~MockDevice() override = default;
};
