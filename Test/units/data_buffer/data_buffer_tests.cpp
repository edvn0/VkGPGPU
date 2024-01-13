#include "Allocator.hpp"
#include "DataBuffer.hpp"
#include "Image.hpp"
#include "ImageProperties.hpp"
#include "Types.hpp"

#include <catch2/catch_test_macros.hpp>
#include <numeric>

TEST_CASE("General use cases of a databuffer", "[databuffer]") {
  SECTION("Allocation of a databuffer is movable") {
    Core::DataBuffer databuffer{10};
    REQUIRE(!databuffer.valid());
  }

  SECTION("Allocation of a databuffer is movable") {
    INFO("HERE 1 ");
    Core::DataBuffer databuffer{10};
    REQUIRE(!databuffer.valid());

    {
      std::array<Core::u8, 10> data{};
      data.fill(7);
      INFO("HERE 2 ");

      databuffer.write(data.data(), 10);
      INFO("HERE");
    }
    REQUIRE(databuffer.valid());
    {
      Core::DataBuffer copy = Core::DataBuffer::copy(databuffer);
      REQUIRE(copy.valid());
      REQUIRE(copy.size() == databuffer.size());
    }
  }

  SECTION("Reading the databuffer is possible") {
    Core::DataBuffer databuffer{10};
    REQUIRE(!databuffer.valid());

    {
      std::array<Core::u8, 10> data{};
      // 1, 2, 3...
      std::iota(data.begin(), data.end(), 1);

      databuffer.write(data.data(), 10);
    }
    REQUIRE(databuffer.valid());

    {
      std::array<Core::u8, 10> data{};
      std::span output{data};
      databuffer.read(output, 10);
      REQUIRE(data[0] == 1);
      REQUIRE(data[1] == 2);
      REQUIRE(data[2] == 3);
      REQUIRE(data[9] == 10);
    }

    {
      std::array<Core::u8, 10> data{};
      databuffer.read(data);
      REQUIRE(data[0] == 1);
      REQUIRE(data[1] == 2);
      REQUIRE(data[2] == 3);
      REQUIRE(data[9] == 10);
    }

    {
      std::vector<Core::u8> data{10};
      REQUIRE_THROWS_AS(databuffer.read(data, 10), Core::WriteRangeException);
    }

    {
      std::vector<Core::u8> data{};
      data.resize(10);
      databuffer.read(data, 10);
      REQUIRE(data[0] == 1);
      REQUIRE(data[1] == 2);
      REQUIRE(data[2] == 3);
      REQUIRE(data[9] == 10);
    }
  }
}
