#include "Allocator.hpp"
#include "CommandBufferThreadPool.hpp"
#include "Texture.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <future>

#include "common/device_mock.hpp"
#include "common/instance_mock.hpp"
#include "common/window_mock.hpp"

static auto make_white_texture(const Core::Device *device,
                               Core::CommandBuffer &buffer) {
  using namespace Core;
  DataBuffer white_data(sizeof(u32));
  u32 white = 0xFFFFFFFF;
  white_data.write(&white, sizeof(u32));
  return Texture::construct_from_command_buffer(
      *device,
      {
          .format = ImageFormat::UNORM_RGBA8,
          .extent =
              {
                  1,
                  1,
              },
          .usage = ImageUsage::Sampled | ImageUsage::TransferDst |
                   ImageUsage::TransferSrc,
          .layout = ImageLayout::ShaderReadOnlyOptimal,
          .address_mode = SamplerAddressMode::ClampToEdge,
          .border_color = SamplerBorderColor::FloatOpaqueWhite,
      },
      std::move(white_data), buffer);
}

TEST_CASE("CommandBufferThreadPool", "[CBTP]") {
  MockInstance instance{};
  MockWindow window{instance};
  MockDevice device{instance, window};
  Core::Allocator::construct(device, instance);

  Core::CommandBufferThreadPool<Core::Texture> pool{2, device};

  // This is an integration test, using a real vulkan context with a real
  // logical device. What I want, is to be able to submit three async tasks that
  // loads a texture via Texture::construct_shader and executes them, and
  // returns from the pool with a std::future<std::unique_ptr<Texture>>. The API
  // should look like following
  using namespace Core;
  using Future = std::future<Scope<Texture>>;
  std::queue<Future> futures;

  for (const auto i : std::views::iota(0, 3)) {
    futures.emplace(pool.submit([&](CommandBuffer &buffer) -> Scope<Texture> {
      return make_white_texture(&device, buffer);
    }));
  }

  std::unordered_map<std::string, Scope<Texture>> data;
  // Wait for the futures to complete
  while (!futures.empty()) {
    try {

      auto &future = futures.front();
      auto texture = future.get();
      data.emplace("texture" + std::to_string(data.size()), std::move(texture));
      futures.pop();
    } catch (const std::exception &e) {
      // Fail the test Catch2
      FAIL(e.what());
    }
  }

  REQUIRE(data.size() == 3);
  // Check that the textures are not nullptr
  for (const auto &[name, texture] : data) {
    REQUIRE(texture != nullptr);
  }
}

TEST_CASE("CommandBufferThreadPool but 1000 textures", "[CBTP]") {
  MockInstance instance{};
  MockWindow window{instance};
  MockDevice device{instance, window};
  Core::Allocator::construct(device, instance);

  Core::CommandBufferThreadPool<Core::Texture> pool{12, device};

  using namespace Core;
  using Future = std::future<Scope<Texture>>;
  std::queue<Future> futures;

  for (const auto i : std::views::iota(0, 1000)) {
    futures.emplace(pool.submit([&](CommandBuffer &buffer) -> Scope<Texture> {
      return make_white_texture(&device, buffer);
    }));
  }

  std::unordered_map<std::string, Scope<Texture>> data;
  // Wait for the futures to complete
  while (!futures.empty()) {
    try {

      auto &future = futures.front();
      auto texture = future.get();
      data.emplace("texture" + std::to_string(data.size()), std::move(texture));
      futures.pop();
    } catch (const std::exception &e) {
      FAIL(e.what());
    }
  }

  REQUIRE(data.size() == 3);
  // Check that the textures are not nullptr
  for (const auto &[name, texture] : data) {
    REQUIRE(texture != nullptr);
  }
}
