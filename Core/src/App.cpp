#include "App.hpp"

#include "pch/vkgpgpu_pch.hpp"

#include "Config.hpp"
#include "Formatters.hpp"
#include "Logger.hpp"

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

  [[nodiscard]] auto should_print() const -> bool {
    return (frame_counter + 1) % N == 0;
  }

  auto print() const -> void {
    const auto avg_frame_time = frame_time_sum / N;
    const auto fps = 1.0 / avg_frame_time;

    // Log average frame time and FPS
    info("Average Frame Time: {:.6f} ms, FPS: {:.0f}", avg_frame_time * 1000.0,
         fps);
  }
};

namespace Core {

App::App(const ApplicationProperties &props) : properties(props) {}

auto App::frame() const -> u32 { return current_frame; }

auto App::run() -> void {
  static constexpr auto now = [] {
    return std::chrono::high_resolution_clock::now();
  };

  try {
    FPSAverage fps_average; // Object for tracking the frames per second.
    on_create();            // Initialize your application.

    auto last_time = now();            // Record the starting time.
    const auto total_time = last_time; // Store the total time of the app.

    // Main loop
    while (running) {
      const auto current_time =
          now(); // Get the current time at the start of the loop.

      // Calculate the time delta since the last frame.
      const auto delta_time_seconds =
          std::chrono::duration<double>(current_time - last_time).count();

      // Update the fps average.
      fps_average.update();

      // Print the fps if needed.
      if (fps_average.should_print()) {
        fps_average.print();
      }

      // Call the update function with the time delta.
      on_update(delta_time_seconds);

      // Update the frame count.
      current_frame = (current_frame + 1) % Config::frame_count;

      // Update the last_time to the current time after the frame update.
      last_time = current_time;
    }

    // Calculate and log the total runtime.
    info("Total time: {} seconds.",
         std::chrono::duration<double>(now() - total_time).count());
    on_destroy(); // Clean up resources.

  } catch (const std::exception &exc) {
    error("Main loop exception: {}", exc);
  }
}

} // namespace Core
