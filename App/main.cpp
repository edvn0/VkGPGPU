#include "Allocator.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DescriptorMap.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Pipeline.hpp"
#include "DebugMarker.hpp"
#include "Shader.hpp"
#include "Types.hpp"
#include <filesystem>
#include <span>

#include <array>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <vulkan/vulkan.h>

#include <fmt/format.h>

#include <renderdoc_app.h>
#include <Windows.h>


#include <atomic>
#include <csignal>

RENDERDOC_API_1_0_0* GetRenderDocApi()
{
  Core::DebugMarker::setup(Core::Device::get()->get_device(), Core::Device::get()->get_physical_device());

  RENDERDOC_API_1_1_2 *rdoc_api = NULL;

  // At init, on windows
  HMODULE mod = GetModuleHandleA("renderdoc.dll");
  if(mod != nullptr)
  {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
    assert(ret == 1);
  }

  pRENDERDOC_GetAPI getApi = nullptr;
  getApi = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");

  if (getApi == nullptr)
  {
    return nullptr;
  }

  if (getApi(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api) != 1)
  {
    return nullptr;
  }

  return rdoc_api;
}


template <Core::u64 N, Core::u64 M> struct Matrix {
  std::array<std::array<float, N>, M> data;
};

auto randomize_span_of_matrices(std::span<Matrix<4, 4>> matrices) -> void {
  static constexpr auto xy_to_index = [](Matrix<4, 4> &matrix, Core::u64 x,
                                         Core::u64 y) -> float & {
    return matrix.data.at(x).at(y);
  };

  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> dis(-1.0F, 1.0F);

  for (auto &matrix : matrices) {
    xy_to_index(matrix, 0, 0) = dis(gen);
    xy_to_index(matrix, 0, 1) = dis(gen);
    xy_to_index(matrix, 0, 2) = dis(gen);
    xy_to_index(matrix, 0, 3) = dis(gen);
    xy_to_index(matrix, 1, 0) = dis(gen);
    xy_to_index(matrix, 1, 1) = dis(gen);
    xy_to_index(matrix, 1, 2) = dis(gen);
    xy_to_index(matrix, 1, 3) = dis(gen);
    xy_to_index(matrix, 2, 0) = dis(gen);
    xy_to_index(matrix, 2, 1) = dis(gen);
    xy_to_index(matrix, 2, 2) = dis(gen);
    xy_to_index(matrix, 2, 3) = dis(gen);
    xy_to_index(matrix, 3, 0) = dis(gen);
    xy_to_index(matrix, 3, 1) = dis(gen);
    xy_to_index(matrix, 3, 2) = dis(gen);
    xy_to_index(matrix, 3, 3) = dis(gen);
  }
}

void perform(auto* renderdoc) {

  Core::CommandBuffer command_buffer(Core::CommandBuffer::Type::Compute);

  std::array<std::string, 2> keys{"LOG_LEVEL", "ENABLE_VALIDATION_LAYERS"};
  Core::Environment::initialize(keys);

  using mat4 = Matrix<4, 4>;
  std::array<mat4, 10> matrices{};
  std::array<mat4, 10> output_matrices{};
  static constexpr auto log_matrices = [](std::span<mat4> matrices) -> void {
    for (const auto &matrix : matrices) {
      info("Matrix:");
      info("{} {} {} {}", matrix.data.at(0).at(0), matrix.data.at(0).at(1),
           matrix.data.at(0).at(2), matrix.data.at(0).at(3));
      info("{} {} {} {}", matrix.data.at(1).at(0), matrix.data.at(1).at(1),
           matrix.data.at(1).at(2), matrix.data.at(1).at(3));
      info("{} {} {} {}", matrix.data.at(2).at(0), matrix.data.at(2).at(1),
           matrix.data.at(2).at(2), matrix.data.at(2).at(3));
      info("{} {} {} {}", matrix.data.at(3).at(0), matrix.data.at(3).at(1),
           matrix.data.at(3).at(2), matrix.data.at(3).at(3));
    }
  };

  randomize_span_of_matrices(matrices);

  auto input_buffer =
      Core::Buffer{matrices.size() * sizeof(mat4), Core::Buffer::Type::Storage};
  auto output_buffer = Core::Buffer{output_matrices.size() * sizeof(mat4),
                                    Core::Buffer::Type::Storage};

  struct Uniform {
    float whatever{};
  } uniform{};
  auto simple_uniform =
      Core::Buffer{sizeof(Uniform), Core::Buffer::Type::Uniform};

  Core::DescriptorMap descriptor_map{};
  descriptor_map.add_for_frames(0, 0, input_buffer);
  descriptor_map.add_for_frames(0, 1, output_buffer);
  descriptor_map.add_for_frames(0, 2, simple_uniform);

  Core::Shader shader{Core::FS::shader("MatrixMultiply.comp.spv")};
  Core::Pipeline pipeline({
      "MatrixMultiply",
      Core::PipelineStage::Compute,
      &shader,
      descriptor_map.get_descriptor_set_layout(),
  });

  Core::u32 frame = 0;
  Core::CommandDispatcher dispatcher(&command_buffer);

  // Lets do a basic benchmark, mean and stdev of computation times
  for (auto i = 0U; i < 100; i++) {
    if (renderdoc != nullptr) {

    renderdoc->StartFrameCapture(nullptr, nullptr);
    }

    randomize_span_of_matrices(matrices);
    input_buffer.write(matrices.data(), matrices.size() * sizeof(mat4));
    simple_uniform.write(&uniform, sizeof(Uniform));
    // Begin command buffer
    command_buffer.begin(frame);
    // Bind pipeline
    Core::DebugMarker::begin_region(command_buffer.get_command_buffer(),
                                    "MatrixMultiply", {1.0F, 0.0F, 0.0F, 1.0F});
    dispatcher.bind(pipeline);
    // Bind descriptor sets
    descriptor_map.bind(command_buffer, frame, pipeline.get_pipeline_layout());
    // Dispatch
    dispatcher.dispatch({
        .group_count_x = 256,
        .group_count_y = 1,
        .group_count_z = 1,
    });
    // End command buffer
    Core::DebugMarker::end_region(command_buffer.get_command_buffer());
    command_buffer.end_and_submit();
    frame = (frame + 1) % Core::Config::frame_count;

    if (renderdoc != nullptr) {
    renderdoc->EndFrameCapture(nullptr, nullptr);

    }
  }
}

int main(int argc, char **argv) {
  const std::span args(argv, argc);
  std::vector<std::string_view> views{};
  for (const auto &arg : args) {
    views.emplace_back(arg);
  }

  auto rdoc = GetRenderDocApi();
  perform(rdoc);

  info("Exiting");

  Core::Allocator::destroy();
  Core::Device::destroy();
  Core::Instance::destroy();

  // Close dll


  return 0;
}
