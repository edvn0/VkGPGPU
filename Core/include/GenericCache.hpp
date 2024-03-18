#pragma once

#include "CommandBuffer.hpp"
#include "CommandBufferThreadPool.hpp"
#include "Concepts.hpp"
#include "Containers.hpp"
#include "Device.hpp"
#include "ThreadPool.hpp"
#include "Types.hpp"

#include <future>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>

#include "core/Forward.hpp"

namespace Core {

template <typename R> bool is_ready(const std::future<R> &f) {
  return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <class Constructor>
concept ConstructorLike =
    requires(const Device &device,
             const typename Constructor::Properties &properties) {
      {
        Constructor::construct(device, properties)
      } -> std::same_as<Scope<typename Constructor::Type>>;
    } ||
    requires(const Device &device,
             const typename Constructor::Properties &properties,
             CommandBuffer &command_buffer) {
      {
        Constructor::construct_from_command_buffer(device, properties,
                                                   command_buffer)
      } -> std::same_as<Scope<typename Constructor::Type>>;
    };

template <class T, class P>
concept Cacheable = requires(const Device &device, const P &properties) {
  { properties.identifier } -> std::convertible_to<std::string>;
};

template <class T, class P> struct DefaultConstructor {
  using Type = T;
  using Properties = P;

  static auto construct(const Device &device, const P &properties) -> Scope<T> {
    return T::construct(device, properties);
  }

  static auto construct_from_command_buffer(const Device &device,
                                            const P &properties,
                                            CommandBuffer &command_buffer)
      -> Scope<T> {
    return T::construct_from_command_buffer(device, properties, command_buffer);
  }
};

/**
 * @brief A cache for objects of type T, with asynchronous loading capabilities.
 *
 * @tparam T The type of objects to cache.
 * @tparam P The properties type used for object identification and
 * construction
 * @tparam IsAsynchronous Use the async version of this cache
 * @tparam C A constructor-like class for creating objects of type T. Must
 * satisfy ConstructorLike concept. The ConstructorLike concept is satisfied by
 * any class with a static construct function that returns a Scope<T> and takes
 * a const Device & and a const P & as arguments.
 */
template <class T, class P, bool IsAsynchronous = true,
          ConstructorLike C = DefaultConstructor<T, P>>
  requires(Cacheable<T, P>)
class GenericCache {
public:
  explicit GenericCache(const Device &dev, Scope<Texture> loading_texture,
                        usize thread_count = 12)
      : device(&dev), loading(std::move(loading_texture)),
        command_buffer_pool(thread_count, dev) {}

  auto put_or_get(const P &props) -> const Scope<T> & {
    std::scoped_lock lock(access);
    if (processing_identifier_cache.contains(props.identifier)) {
      return loading;
    }

    if (type_cache.contains(props.identifier)) {
      return type_cache.at(props.identifier);
    } else {
      if constexpr (IsAsynchronous) {
        processing_identifier_cache.emplace(props.identifier);
        auto future_resource = command_buffer_pool.submit(
            [this, props](CommandBuffer &cmd_buffer) {
              return C::construct_from_command_buffer(*device, props,
                                                      cmd_buffer);
            });
        future_tasks.emplace(props.identifier, std::move(future_resource));
        return loading;
      } else {
        type_cache[props.identifier] = C::construct(*device, props);
        return type_cache.at(props.identifier);
      }
    }
  }

  void update_one() {
    std::scoped_lock lock{access};
    auto maybe = pop_first_no_copy(future_tasks);
    if (!maybe)
      return;
    auto &&[key, future] = maybe.value();
    type_cache.try_emplace(key, std::move(future.get()));
    processing_identifier_cache.erase(key);
  }

  template <typename F> void update_one(F &&post_insert_hook) {
    std::scoped_lock lock{access};
    auto maybe = pop_first_no_copy(future_tasks);
    if (!maybe)
      return;
    auto &&[key, future] = maybe.value();
    type_cache.try_emplace(key, std::move(future.get()));

    auto &value = type_cache.at(key);
    post_insert_hook(value);

    processing_identifier_cache.erase(key);
  }

private:
  const Device *device;
  Container::StringLikeMap<Scope<T>> type_cache;
  std::map<std::string, std::future<Scope<T>>> future_tasks;
  std::unordered_set<std::string> processing_identifier_cache;
  std::mutex access;
  Scope<Texture> loading;
  CommandBufferThreadPool<T> command_buffer_pool;

  template <typename K, typename V>
  std::optional<std::pair<K, V>> pop_first_no_copy(std::map<K, V> &map) {
    if (map.empty()) {
      return {}; // Return an empty optional if the map is empty
    }
    auto it = map.begin();
    if (!is_ready(it->second))
      return {};

    // Create a pair with a copy of the key and a move of the value
    std::optional<std::pair<K, V>> result =
        std::make_pair(it->first, std::move(it->second));
    map.erase(it); // Erase the first element
    return result; // Return the result without copying the value
  }
};

} // namespace Core
