#include "Allocator.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DebugMarker.hpp"
#include "DescriptorMap.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Pipeline.hpp"
#include "Shader.hpp"
#include "Timer.hpp"
#include "Types.hpp"
#include <span>

#include <array>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

#include <fmt/format.h>
#include <vulkan/vulkan_core.h>

#ifdef _WIN32
static constexpr std::string_view renderdoc_dll_name = "renderdoc.dll";
#else
static constexpr std::string_view renderdoc_dll_name = "librenderdoc.so";
#endif

#if !defined(GPGPU_PIPELINE)
#include <renderdoc_app.h>

auto GetRenderDocApi(auto &device, auto &loader) -> RENDERDOC_API_1_6_0 * {
  Core::DebugMarker::setup(device.get_device(), device.get_physical_device());

  if (!loader->is_valid()) {
    info("Dynamic loader could not be constructed.");
    return nullptr;
  }

  auto RENDERDOC_GetAPI =
      std::bit_cast<pRENDERDOC_GetAPI>(loader->get_symbol("RENDERDOC_GetAPI"));
  RENDERDOC_API_1_1_2 *rdoc_api = nullptr;

  if (RENDERDOC_GetAPI &&
      RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api) == 1) {
    return rdoc_api;
  }

  info("Could not load symbols.");
  return nullptr;
}
#endif

template <Core::u64 N, Core::u64 M> struct Matrix {
  std::array<std::array<float, N>, M> data{};
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

void perform(const Core::Scope<Core::Device> &device, void *renderdoc) {

  Core::CommandBuffer command_buffer(*device,
                                     Core::CommandBuffer::Type::Compute);

  using mat4 = Matrix<4, 4>;
  std::array<mat4, 10> matrices{};
  std::array<mat4, 10> output_matrices{};
  randomize_span_of_matrices(matrices);

  auto input_buffer = Core::Buffer{*device, matrices.size() * sizeof(mat4),
                                   Core::Buffer::Type::Storage};
  auto other_input_buffer = Core::Buffer{
      *device, matrices.size() * sizeof(mat4), Core::Buffer::Type::Storage};
  auto output_buffer =
      Core::Buffer{*device, output_matrices.size() * sizeof(mat4),
                   Core::Buffer::Type::Storage};

  struct Uniform {
    float whatever{};
  };
  Uniform uniform{};
  auto simple_uniform =
      Core::Buffer{*device, sizeof(Uniform), Core::Buffer::Type::Uniform};

  Core::DescriptorMap descriptor_map{*device};
  descriptor_map.add_for_frames(0, 0, input_buffer);
  descriptor_map.add_for_frames(0, 1, input_buffer);
  descriptor_map.add_for_frames(0, 2, output_buffer);
  descriptor_map.add_for_frames(0, 3, simple_uniform);

  Core::Shader shader{
      *device,
      Core::FS::shader("MatrixMultiply.comp.spv"),
  };
  Core::Pipeline pipeline(*device,
                          {
                              "MatrixMultiply",
                              Core::PipelineStage::Compute,
                              shader,
                              descriptor_map.get_descriptor_set_layout(),
                          });

  Core::u32 frame = 0;
  Core::CommandDispatcher dispatcher(&command_buffer);

  for (auto i = 0U; i < Core::Config::frame_count * 1000; i++) {
    Core::Timer timer;

#if !defined(GPGPU_PIPELINE)
    if (renderdoc != nullptr) {
      static_cast<RENDERDOC_API_1_6_0 *>(renderdoc)->StartFrameCapture(nullptr,
                                                                       nullptr);
    }
#endif

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

#if !defined(GPGPU_PIPELINE)
    if (renderdoc != nullptr) {
      static_cast<RENDERDOC_API_1_6_0 *>(renderdoc)->EndFrameCapture(nullptr,
                                                                     nullptr);
    }
#endif
  }

#if !defined(GPGPU_PIPELINE)
  if (renderdoc != nullptr) {
    static_cast<RENDERDOC_API_1_6_0 *>(renderdoc)->RemoveHooks();
  }
#endif
}

int main(int argc, char **argv) {
  const std::span args(argv, argc);
  std::vector<std::string_view> views{};
  for (const auto &arg : args) {
    views.emplace_back(arg);
  }

  // Set current path to "--wd" argument of views, if there is one
  if (auto wd = std::ranges::find(views, "--wd");
      wd != views.end() && ++wd != views.end()) {
    Core::FS::set_current_path(*wd);
  }

  std::array<std::string, 2> keys{"LOG_LEVEL", "ENABLE_VALIDATION_LAYERS"};
  Core::Environment::initialize(keys);

  auto instance = Core::Instance::construct();
  auto device = Core::Device::construct(*instance);
  Core::Allocator::construct(*device, *instance);

  auto loader = Core::DynamicLibraryLoader::construct(renderdoc_dll_name);
#if !defined(GPGPU_PIPELINE)
  auto rdoc = GetRenderDocApi(*device, loader);
  if (rdoc != nullptr) {
    rdoc->SetCaptureTitle("Matrix Multiply");
  }
  perform(device, rdoc);
#else
  perform(device, nullptr);
#endif

  Core::Allocator::destroy();
  device.reset();
  instance.reset();

  // Close dll
  info("Exiting");
  return 0;
}
