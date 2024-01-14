#pragma once

#include "Device.hpp"

class MockDevice : public Core::Device {
public:
  using Core::Device::Device;
  ~MockDevice() override = default;
};
