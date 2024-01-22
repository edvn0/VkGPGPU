#pragma once

#include "Concepts.hpp"
#include "Containers.hpp"
#include "Device.hpp"
#include "Texture.hpp"
#include "Types.hpp"

#include <future>
#include <string>

namespace Core {

template <class T, class P>
concept ConstructorLike = requires(const Device &device, const P &properties) {
  { T::construct(device, properties) } -> std::same_as<Scope<T>>;
};

template <class T, class P>
concept Cacheable = requires(const Device &device, const P &properties) {
  { properties.identifier } -> std::convertible_to<std::string>;
};

struct DefaultConstructor {
  template <class T, class P>
  static auto construct(const Device &device, const P &properties) -> Scope<T> {
    return T::construct(device, properties);
  }
};

template <class T, class P, typename C = DefaultConstructor>
  requires(Cacheable<T, P>)
class GenericCache {
public:
  explicit GenericCache(const Device &dev, Scope<T> loading_texture)
      : device(&dev), loading(std::move(loading_texture)) {}

  auto put_or_get(const P &props) -> const Scope<T> & {
    std::scoped_lock lock(access);
    if (auto it = type_cache.find(props.identifier); it != type_cache.end()) {
      return it->second;
    } else if (auto future_it = future_cache.find(props.identifier);
               future_it != future_cache.end()) {
      if (future_it->second.wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        type_cache[props.identifier] = future_it->second.get();
        future_cache.erase(future_it);
        return type_cache[props.identifier];
      } else {
        return loading;
      }
    } else {
      auto future_texture = std::async(std::launch::async, [this, props]() {
        return C::construct(*device, props);
      });
      future_cache[props.identifier] = std::move(future_texture);
      return loading;
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
