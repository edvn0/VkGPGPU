#pragma once

#include "Instance.hpp"

class MockInstance : public Core::Instance {
public:
  using Core::Instance::Instance;
  ~MockInstance() override = default;
};
