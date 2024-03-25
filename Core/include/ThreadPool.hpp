#pragma once

#include "BS_thread_pool.hpp"
#include "Config.hpp"
#include "Types.hpp"

namespace Core {

class ThreadPool {
public:
  template <typename F, typename R = std::invoke_result_t<std::decay_t<F>>>
  [[nodiscard]] static auto submit(F &&task) -> std::future<R> {
    return pool.submit_task(std::forward<F>(task));
  }

private:
  static constexpr u32 thread_count = Config::thread_count;
  static inline BS::thread_pool pool{thread_count};
};

} // namespace Core
