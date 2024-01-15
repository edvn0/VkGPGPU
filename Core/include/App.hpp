#pragma once

#include "Types.hpp"

#include <atomic>

namespace Core {

struct ApplicationProperties {
  bool headless{true};
};

class App {
public:
  auto run() -> void;
  virtual ~App() = default;

protected:
  virtual auto on_update(double ts) -> void = 0;
  virtual auto on_create() -> void = 0;
  virtual auto on_destroy() -> void = 0;

  explicit App(const ApplicationProperties &);

  [[nodiscard]] auto frame() const -> u32;

private:
  ApplicationProperties properties{};

  u32 current_frame{0};
  bool running{true};
};

auto extern make_application(const ApplicationProperties &) -> Scope<App>;

} // namespace Core
