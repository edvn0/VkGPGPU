#include "App.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Config.hpp"
#include "Logger.hpp"

#include <csignal>

namespace Core {

App::App(const ApplicationProperties &props) : properties(props) {
  std::signal(SIGINT, signal_handler);
}

auto App::signal_handler(int signal) -> void {
  if (signal == SIGINT) {
    graceful_exit_requested = true;
    info("Graceful exit requested.");
  }
}

auto App::run() -> void {
  static constexpr auto delta_time_in_seconds = [](const auto &current,
                                                   const auto &last) {
    return std::chrono::duration<double>(current - last).count();
  };

  try {
    on_create();

    auto last_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::high_resolution_clock::now();
    while (running && !graceful_exit_requested) {
      const auto current_time = std::chrono::high_resolution_clock::now();
      const auto delta_time_seconds =
          delta_time_in_seconds(current_time, last_time);
      last_time = current_time;

      on_update(delta_time_seconds);
      current_frame = (current_frame + 1) % Config::frame_count;
    }

    info("Total time: {} seconds.",
         std::chrono::duration<double>(
             std::chrono::high_resolution_clock::now() - total_time)
             .count());

    on_destroy();
  } catch (...) {
    error("Some error happened.");
  }
}

} // namespace Core
