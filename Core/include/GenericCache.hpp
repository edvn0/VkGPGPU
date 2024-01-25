#pragma once

#include "Concepts.hpp"
#include "Containers.hpp"
#include "Device.hpp"
#include "Texture.hpp"
#include "ThreadPool.hpp"
#include "Types.hpp"

#include <future>
#include <queue>
#include <string>
#include <unordered_set>

namespace Core {

template <typename R> bool is_ready(const std::future<R> &f) {
  return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <class Constructor>
concept ConstructorLike = requires(
    const Device &device, const typename Constructor::Properties &properties) {
  {
    Constructor::construct(device, properties)
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
};

/**
 * @brief A cache for objects of type T, with asynchronous loading capabilities.
 *
 * @tparam T The type of objects to cache.
 * @tparam P The properties type used for object identification and
 * construction
 * @tparam IsAsynchronous Use the sync version of this cache
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
  /**
   * @brief Construct a new Generic Cache object.
   *
   * @param dev Reference to the device used for object construction.
   * @param loading_texture The texture to return while objects are loading.
   */
  explicit GenericCache(const Device &dev, Scope<T> loading_texture)
      : device(&dev), loading(std::move(loading_texture)) {}

  /**
   * @brief Get or load an object of type T, identified by properties of type P.
   *
   * If the object is not in the cache, it will be loaded asynchronously.
   *
   * @param props The properties identifying the object.
   * @return const Scope<T>& A reference to the requested object, or a loading
   * texture if loading.
   */
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
        auto future_texture = ThreadPool::submit(
            [this, props]() { return C::construct(*device, props); });

        future_cache.emplace(CacheTask{
            .identifier = props.identifier,
            .future = std::move(future_texture),
        });
        return loading;
      } else {
        type_cache[props.identifier] = C::construct(*device, props);
        return type_cache.at(props.identifier);
      }
    }
  }

  auto update_one() {
    if (future_cache.empty()) {
      return;
    }

    std::scoped_lock lock(access);
    auto &&[identifier, future] = future_cache.front();
    // Check state of future
    if (is_ready(future)) {
      return;
    }

    auto result = future.get();
    type_cache[identifier] = std::move(result);
    future_cache.pop();
    processing_identifier_cache.erase(identifier);
  }

  auto type_cache_size() const -> usize { return type_cache.size(); }
  auto future_cache_size() const -> usize { return future_cache.size(); }

  auto get_loading_texture() const -> const Scope<T> & { return loading; }

private:
  const Device *device;

  struct CacheTask {
    std::string identifier;
    std::future<Scope<T>> future;
  };

  std::queue<CacheTask> future_cache;
  std::unordered_set<std::string> processing_identifier_cache;
  Container::StringLikeMap<Scope<T>> type_cache;
  std::mutex access;
  std::mutex other;
  Scope<T> loading;
};

} // namespace Core
