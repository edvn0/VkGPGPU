#pragma once

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Core::Container {

struct StringLikeHasher {
  using is_transparent = void; // This enables heterogeneous lookup

  size_t operator()(const std::string &s) const {
    return std::hash<std::string>{}(s);
  }

  size_t operator()(const std::string_view sv) const {
    return std::hash<std::string_view>{}(sv);
  }
};

struct StringLikeEqual {
  using is_transparent = void; // This enables heterogeneous lookup

  bool operator()(const std::string &lhs, const std::string &rhs) const {
    return lhs == rhs;
  }

  bool operator()(const std::string_view lhs,
                  const std::string_view rhs) const {
    return lhs == rhs;
  }
};

template <typename Value>
using StringLikeMap =
    std::unordered_map<std::string, Value, StringLikeHasher, StringLikeEqual>;

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
  std::end(t);
  std::cend(t);
  std::begin(t);
  std::cbegin(t);
  std::size(t);
  std::empty(t);
  std::data(t);
};

auto sort(Container auto &container) -> void {
  std::ranges::sort(std::begin(container), std::end(container));
}

auto sort(Container auto &container, auto &&predicate) -> void {
  std::ranges::sort(std::begin(container), std::end(container), predicate);
}

template <class T, std::integral IndexType = std::size_t>
  requires(std::is_nothrow_constructible_v<T> &&
           std::is_default_constructible_v<T>)
class CircularBuffer {
  using Ty = IndexType;
  Ty head{0};
  Ty tail{0};
  Ty count{0};
  std::vector<T> storage;

public:
  explicit CircularBuffer(Ty size) : storage(size) {}

  // Destructor (default)
  ~CircularBuffer() = default;

  // Copy constructor
  CircularBuffer(const CircularBuffer &other)
      : storage(other.storage), head(other.head), tail(other.tail),
        count(other.count) {}

  // Copy assignment operator
  CircularBuffer &operator=(const CircularBuffer &other) {
    if (this != &other) {
      storage = other.storage;
      head = other.head;
      tail = other.tail;
      count = other.count;
    }
    return *this;
  }

  // Move constructor
  CircularBuffer(CircularBuffer &&other) noexcept
      : storage(std::move(other.storage)), head(other.head), tail(other.tail),
        count(other.count) {}

  // Move assignment operator
  CircularBuffer &operator=(CircularBuffer &&other) noexcept {
    if (this != &other) {
      storage = std::move(other.storage);
      other.storage.clear();
      head = other.head;
      tail = other.tail;
      count = other.count;
    }
    return *this;
  }

  auto push(const T &item) noexcept -> void {
    storage[head] = item;
    head = (head + 1) % storage.size();
    if (count < storage.size()) {
      count++;
    } else {
      tail = (tail + 1) % storage.size();
    }
  }

  auto push(T &&item) noexcept -> void {
    storage[head] = std::move(item);
    head = (head + 1) % storage.size();
    if (count < storage.size()) {
      count++;
    } else {
      tail = (tail + 1) % storage.size();
    }
  }

  template <class... Ts> auto emplace(Ts &&...args) noexcept -> void {
    storage[head] = T(std::forward<Ts>(args)...);
    head = (head + 1) % storage.size();
    if (count < storage.size()) {
      count++;
    } else {
      tail = (tail + 1) % storage.size();
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

    T item = std::move(storage[tail]);
    tail = (tail + 1) % storage.size();
    count--;
    return item;
  }

  auto peek() const -> const T & { return storage[tail]; }

  auto size() const noexcept -> Ty { return count; }

  [[nodiscard]] auto empty() const noexcept -> bool { return count == 0; }

  [[nodiscard]] auto full() const noexcept -> bool {
    return count == storage.size();
  }
};

} // namespace Core::Container
