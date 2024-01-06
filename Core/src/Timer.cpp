#include "Timer.hpp"
#include <fstream>

#ifndef GPGPU_RETAIN_SECONDS
#define GPGPU_BUFFER_SECONDS 0.5
#endif

namespace Core {

std::vector<Timer::BufferSize> Timer::buffer(Timer::buffer_size);
std::chrono::time_point<std::chrono::high_resolution_clock>
    Timer::last_write_time = std::chrono::high_resolution_clock::now();
size_t Timer::current_index = 0;
size_t Timer::last_write_index = 0;
std::mutex Timer::buffer_mutex;
static constexpr auto loops_per_second = 20000;
const size_t Timer::buffer_size = GPGPU_BUFFER_SECONDS * loops_per_second;

Timer::Timer() { start_time = std::chrono::high_resolution_clock::now(); }

Timer::~Timer() {
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start_time)
          .count();

  add_duration(duration);

  if (should_write_to_file()) {
    write_to_file();
    last_write_time =
        std::chrono::high_resolution_clock::now(); // Update the last write time
  }
}

void Timer::add_duration(Timer::BufferSize duration) {
  std::lock_guard<std::mutex> lock(buffer_mutex);
  buffer[current_index] = duration;
  current_index = (current_index + 1) % buffer_size;
}

bool Timer::should_write_to_file() {
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - last_write_time)
          .count();
  return elapsed >= 2; // Check if 2 seconds have elapsed
}

void Timer::write_to_file() {
  std::lock_guard<std::mutex> lock(buffer_mutex);
  std::ofstream file("timings.txt", std::ios::app);

  size_t index = last_write_index;
  while (index != current_index) {
    file << buffer[index] << std::endl;
    index = (index + 1) % buffer_size;
  }

  last_write_index = current_index; // Update the last write index after writing
}

} // namespace Core