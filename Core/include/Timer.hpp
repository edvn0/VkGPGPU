#pragma once

#include <chrono>
#include <mutex>
#include <vector>

namespace Core {

class Timer {
  using BufferSize = long long;

public:
  Timer();
  ~Timer();

private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  static std::vector<BufferSize> buffer;
  static const size_t buffer_size;
  static size_t current_index;
  static size_t last_write_index;
  static std::mutex buffer_mutex;
  static std::chrono::time_point<std::chrono::high_resolution_clock>
      last_write_time;

  void add_duration(BufferSize duration);
  bool should_write_to_file();
  void write_to_file();
};

} // namespace Core