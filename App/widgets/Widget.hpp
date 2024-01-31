#pragma once

#include "InterfaceSystem.hpp"
#include "Types.hpp"

class Widget {
public:
  virtual ~Widget() = default;
  virtual void on_update(Core::floating ts) = 0;
  virtual void on_interface(Core::InterfaceSystem &) = 0;
  virtual void on_create() = 0;
  virtual void on_destroy() = 0;
};
