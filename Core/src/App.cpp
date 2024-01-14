#include "App.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Config.hpp"
#include "Formatters.hpp"
#include "Logger.hpp"

#include <csignal>
#include <cstddef>
#include <exception>

template <std::size_t N = 10000> struct FPSAverage {
  std::array<double, N> frame_times{};
  double frame_time_sum = 0.0;
  Core::i32 frame_time_index = 0;
  Core::i32 frame_counter = 0;

  std::chrono::high_resolution_clock::time_point last_time;
  bool initialized = false;

  auto update() -> void {
    if (!initialized) {
      last_time = std::chrono::high_resolution_clock::now();
      initialized = true;
      return;
    }

    const auto current_time = std::chrono::high_resolution_clock::now();
    const auto delta_time_seconds =
        std::chrono::duration<double>(current_time - last_time).count();
    last_time = current_time;

    // Update moving average for frame time
    frame_time_sum -= frame_times[frame_time_index];
    frame_times[frame_time_index] = delta_time_seconds;
    frame_time_sum += delta_time_seconds;
    frame_time_index = (frame_time_index + 1) % N;

    // Increment frame counter
    frame_counter++;
  }

  auto should_print() const -> bool { return frame_counter % N == 0; }

  auto print() const -> void {
    const auto avg_frame_time = frame_time_sum / N;
    const auto fps = 1.0 / avg_frame_time;

    // Log average frame time and FPS
    info("Average Frame Time: {:.6f} ms, FPS: {:.2f}, Frame counter: {}",
         avg_frame_time * 1000.0, fps, frame_counter);
  }
};

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
  static constexpr auto now = []() {
    return std::chrono::high_resolution_clock::now();
  };

  try {
    FPSAverage fps_average;
    on_create();

    auto last_time = std::chrono::high_resolution_clock::now();
    auto total_time = last_time;
    while (running && !graceful_exit_requested) {
      const auto current_time = std::chrono::high_resolution_clock::now();
      const auto delta_time_seconds =
          std::chrono::duration<double>(current_time - last_time).count();
      fps_average.update();

      if (fps_average.should_print()) {
        fps_average.print();
      }
      last_time = current_time;

      on_update(delta_time_seconds);
    }

    info("Total time: {} seconds.",
         std::chrono::duration<double>(now() - total_time).count());
    on_destroy();

  } catch (const std::exception &exc) {
    error("Main loop exception: {}", exc);
  }
}

} // namespace Core
