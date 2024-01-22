#pragma once

#include "Concepts.hpp"
#include "Containers.hpp"
#include "Device.hpp"
#include "Texture.hpp"
#include "Types.hpp"

#include <future>
#include <string>

namespace Core {

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
 * construction.
 * @tparam C A constructor-like class for creating objects of type T. Must
 * satisfy ConstructorLike concept.
 */
template <class T, class P, ConstructorLike C = DefaultConstructor<T, P>>
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
    if (future_cache.contains(props.identifier)) {
      return loading;
    }

    std::scoped_lock lock(access);
    if (auto it = type_cache.find(props.identifier); it != type_cache.end()) {
      return it->second;
    } else {
      auto future_texture = std::async(std::launch::async, [this, props]() {
        return C::construct(*device, props);
      });
      future_cache[props.identifier] = std::move(future_texture);
      return loading;
    }
  }

  auto update_one() {
    if (auto it = future_cache.begin(); it != future_cache.end()) {
      if (it->second.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        std::scoped_lock static_lock(access);
        type_cache[it->first] = it->second.get();
        future_cache.erase(it);
      }
    }
  }

  auto type_cache_size() const -> usize { return type_cache.size(); }
  auto future_cache_size() const -> usize { return future_cache.size(); }

  auto get_loading() const -> const Scope<T> & { return loading; }

private:
  const Device *device;
  Container::StringLikeMap<std::future<Scope<T>>> future_cache;
  Container::StringLikeMap<Scope<T>> type_cache;
  std::mutex access;
  Scope<T> loading;
};

} // namespace Core
