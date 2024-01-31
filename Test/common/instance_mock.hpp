#pragma once

#include "Instance.hpp"

class MockInstance : public Core::Instance {
public:
  MockInstance() : Core::Instance(true){};
  ~MockInstance() override = default;
};
