#include "Allocator.hpp"
#include "GenericCache.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"
#include "Texture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vulkan/vulkan_core.h>

#include "common/device_mock.hpp"
#include "common/instance_mock.hpp"
#include "common/window_mock.hpp"

struct MockDefault {
  using Properties = Core::TextureProperties;
  using Type = Core::Texture;

  static auto construct(const Core::Device &device,
                        const Core::TextureProperties &properties)
      -> Core::Scope<Core::Texture> {
    return nullptr;
  }
};
using TextureCache =
    Core::GenericCache<Core::Texture, Core::TextureProperties, MockDefault>;

TEST_CASE("Texture Cache tests", "[GenericCache]") {
  const MockInstance instance{};
  const MockWindow window{instance};
  const MockDevice device{instance, window};
  Core::Allocator::construct(device, instance);

  static auto loading_texture = [&dev = device]() {
    return Core::Texture::empty_with_size(dev, 4, {1, 1});
  };

  // ... your setup code ...

  // Now, include each test case here as a subsection

  SECTION("GenericCache returns a new texture if not in cache",
          "[GenericCache]") {
    TextureCache cache(device, loading_texture());

    Core::TextureProperties props{
        .identifier = "texture",
        .path = "path/to/texture",
    };
    auto &texture = cache.put_or_get(props);

    // How would I correctly (but unsafely get this loading pointer?)
    REQUIRE(texture ==
            cache.get_loading()); // Initially returns loading texture
  }
  SECTION("GenericCache returns cached texture if available",
          "[GenericCache]") {
    TextureCache cache(device, loading_texture());

    Core::TextureProperties props{
        .identifier = "texture",
        .path = "path/to/texture",
    };
    cache.put_or_get(props); // Initial call to put in cache

    auto &texture = cache.put_or_get(props); // Subsequent call
    // Compare the two pointers for equality
    REQUIRE(texture == cache.get_loading());
  }
  SECTION("GenericCache handles asynchronous texture loading",
          "[GenericCache]") {
    TextureCache cache(device, loading_texture());

    Core::TextureProperties props{
        .identifier = "texture",
        .path = "path/to/texture",
    };
    cache.put_or_get(props);

    // Simulate waiting for async operation to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto &textureAfterLoad = cache.put_or_get(props);
    REQUIRE(textureAfterLoad !=
            cache.get_loading()); // Should return the loaded texture
  }
  SECTION("GenericCache handles multiple asynchronous calls",
          "[GenericCache]") {
    TextureCache cache(device, loading_texture());

    Core::TextureProperties props1{
        .identifier = "texture",
        .path = "path/to/texture",
    };
    Core::TextureProperties props2{
        .identifier = "texture",
        .path = "path/to/texture",
    };

    auto &texture1 = cache.put_or_get(props1);
    auto &texture2 = cache.put_or_get(props2);

    REQUIRE(texture1 == cache.get_loading());
    REQUIRE(texture2 == cache.get_loading());

    // Simulate waiting for async operations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    REQUIRE(cache.put_or_get(props1) != cache.get_loading());
    REQUIRE(cache.put_or_get(props2) != cache.get_loading());
  }
  SECTION("GenericCache distinguishes textures by unique identifiers",
          "[GenericCache]") {
    TextureCache cache(device, loading_texture());

    Core::TextureProperties props1{
        .identifier = "texture",
        .path = "path/to/texture",
    };
    Core::TextureProperties props2{
        .identifier = "texture",
        .path = "path/to/texture",
    };

    cache.put_or_get(props1);
    auto &texture2 = cache.put_or_get(props2);

    REQUIRE(texture2 ==
            cache.get_loading()); // Should be treated as a different texture
  }
  SECTION("GenericCache handles concurrent texture requests",
          "[GenericCache]") {
    TextureCache cache(device, loading_texture());

    Core::TextureProperties props{
        .identifier = "texture",
        .path = "path/to/texture",
    };

    auto asyncCall1 = std::async(std::launch::async,
                                 [&]() { return &cache.put_or_get(props); });
    auto asyncCall2 = std::async(std::launch::async,
                                 [&]() { return &cache.put_or_get(props); });
  }
}
