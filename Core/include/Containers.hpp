#pragma once

#include <algorithm>
#include <optional>
#include <ranges>

namespace Core::Container {

template <typename T>
concept Container = requires(T t) {
  typename T::value_type;
  typename T::size_type;
  typename T::iterator;
  typename T::const_iterator;
  typename T::reference;
  typename T::const_reference;
  { t.begin() } -> std::convertible_to<typename T::iterator>;
  { t.end() } -> std::convertible_to<typename T::iterator>;
  { t.cbegin() } -> std::convertible_to<typename T::const_iterator>;
  { t.cend() } -> std::convertible_to<typename T::const_iterator>;
  { t.size() } -> std::convertible_to<typename T::size_type>;
  { t.empty() } -> std::convertible_to<bool>;
};

auto sort(Container auto &container) -> void {
  std::ranges::sort(container.begin(), container.end());
}

auto sort(Container auto &container, auto &&predicate) -> void {
  std::ranges::sort(container.begin(), container.end(), predicate);
}

template <class T, std::integral IndexType = usize>
  requires(std::is_nothrow_constructible_v<T> &&
           std::is_default_constructible_v<T>)
class CircularBuffer {
  using Ty = IndexType;
  Ty head{0};
  Ty tail{0};
  Ty capacity;
  Ty count{0};
  std::unique_ptr<T[]> storage{nullptr};

public:
  explicit CircularBuffer(Ty size) noexcept : capacity(size) {
    storage = std::make_unique<T[]>(capacity);
  }

  // Destructor (default)
  ~CircularBuffer() = default;

  // Copy constructor
  CircularBuffer(const CircularBuffer &other)
      : storage(new T[other.capacity]), head(other.head), tail(other.tail),
        capacity(other.capacity), count(other.count) {
    std::copy(&other.storage[0], &other.storage[other.capacity], &storage[0]);
  }

  // Copy assignment operator
  CircularBuffer &operator=(const CircularBuffer &other) {
    if (this != &other) {
      storage = std::make_unique<T[]>(other.capacity);
      std::copy(&other.storage[0], &other.storage[other.capacity], &storage[0]);
      capacity = other.capacity;
      count = other.count;
      head = other.head;
      tail = other.tail;
    }
    return *this;
  }

  // Move constructor
  CircularBuffer(CircularBuffer &&other) noexcept
      : storage(std::move(other.storage)), head(other.head), tail(other.tail),
        capacity(other.capacity), count(other.count) {
    other.capacity = 0;
    other.count = 0;
    other.head = 0;
    other.tail = 0;
  }

  // Move assignment operator
  CircularBuffer &operator=(CircularBuffer &&other) noexcept {
    if (this != &other) {
      storage = std::move(other.storage);
      capacity = other.capacity;
      count = other.count;
      head = other.head;
      tail = other.tail;

      other.capacity = 0;
      other.count = 0;
      other.head = 0;
      other.tail = 0;
    }
    return *this;
  }

  auto push(const T &item) noexcept -> void {
    storage[head] = item;
    head = (head + 1) % capacity;
    if (count < capacity) {
      count++;
    } else {
      tail = (tail + 1) % capacity;
    }
  }

  auto push(T &&item) noexcept -> void {
    storage[head] = std::move(item);
    head = (head + 1) % capacity;
    if (count < capacity) {
      count++;
    } else {
      tail = (tail + 1) % capacity;
    }
  }

  template <class... Ts> auto emplace(Ts &&...args) noexcept -> void {
    storage[head] = T(std::forward<Ts>(args)...);
    head = (head + 1) % capacity;
    if (count < capacity) {
      count++;
    } else {
      tail = (tail + 1) % capacity;
    }
  }

  auto emplace(const Container auto &container) noexcept -> void {
    for (const auto &item : container) {
      push(item);
    }
  }

  auto pop() noexcept -> T {
    if (count == 0) {
      return T();
    }

    T item = storage[tail];
    tail = (tail + 1) % capacity;
    count--;
    return item;
  }

  auto peek() const -> const T & { return storage[tail]; }

  auto size() const noexcept -> Ty { return count; }

  [[nodiscard]] auto empty() const noexcept -> bool { return count == 0; }

  [[nodiscard]] auto full() const noexcept -> bool { return count == capacity; }
};

} // namespace Core::Container
