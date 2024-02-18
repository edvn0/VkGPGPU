#include "Allocator.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"
#include "Texture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <vulkan/vulkan_core.h>

#include "common/device_mock.hpp"
#include "common/instance_mock.hpp"
#include "common/window_mock.hpp"

using namespace Core;

TEST_CASE("MipGeneration Tests", "[MipGeneration]") {
  SECTION("Unused Strategy") {
    MipGeneration gen(MipGenerationStrategy::Unused);
    Extent<u32> extent{1024, 768};
    REQUIRE(determine_mip_count(gen, extent) == 1);
  }

  SECTION("Literal Strategy with specified mips") {
    MipGeneration gen(4); // Literal with 4 mips
    Extent<u32> extent{1024, 768};
    REQUIRE(determine_mip_count(gen, extent) == 4);
  }

  SECTION("Literal Strategy with zero mips defaults to 1") {
    MipGeneration gen(0); // Literal but invalid mips, should default to 1
    Extent<u32> extent{1024, 768};
    REQUIRE(determine_mip_count(gen, extent) == 1);
  }

  SECTION("FromSize Strategy calculates mips based on extent") {
    MipGeneration gen(MipGenerationStrategy::FromSize);
    Extent<u32> extent{1024, 768};
    auto expectedMips =
        calculate_mip_count(extent); // Assume this is the correct calculation
    REQUIRE(determine_mip_count(gen, extent) == expectedMips);
  }
}
