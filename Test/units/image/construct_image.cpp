#include "Allocator.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vulkan/vulkan_core.h>

#include "common/device_mock.hpp"
#include "common/instance_mock.hpp"
#include "common/window_mock.hpp"

TEST_CASE("Construct image", "[image]") {
  MockInstance instance{};
  MockWindow window{instance};
  MockDevice device{instance, window};
  Core::Allocator::construct(device, instance);

  SECTION("Image with correct layout") {
    Core::Image image(device,
                      Core::ImageProperties{
                          .extent = {10, 10},
                          .format = Core::ImageFormat::R8G8B8A8Unorm,
                          .layout = Core::ImageLayout::ShaderReadOnlyOptimal,
                      });

    REQUIRE(image.get_descriptor_info().imageLayout ==
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  SECTION("Image with correct layout") {
    Core::Image image(device,
                      Core::ImageProperties{
                          .extent = {10, 10},
                          .format = Core::ImageFormat::R8G8B8A8Unorm,
                          .layout = Core::ImageLayout::ShaderReadOnlyOptimal,
                      });

    REQUIRE(image.get_descriptor_info().imageLayout ==
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}
