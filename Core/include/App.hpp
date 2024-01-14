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

  static auto frame() -> u32 { return current_frame; }

private:
  ApplicationProperties properties{};

  static auto signal_handler(int signal) -> void;

  static inline u32 current_frame{0};
  static inline bool running{true};
  static inline std::atomic_bool graceful_exit_requested{false};
};

auto extern make_application(const ApplicationProperties &) -> Scope<App>;

} // namespace Core
