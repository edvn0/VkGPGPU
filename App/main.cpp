#include "Buffer.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Logger.hpp"
#include "Pipeline.hpp"
#include "Shader.hpp"
#include "Types.hpp"
#include <filesystem>
#include <span>

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

template <Core::u64 N, Core::u64 M> struct Matrix {
  std::array<std::array<float, N>, M> data;
};

int main(int argc, char **argv) {
  std::span args(argv, argc);
  std::vector<std::string_view> views{};
  for (const auto &arg : args) {
    views.push_back(std::string_view{arg});
  }

  info("Current path: {}", std::filesystem::current_path().string());

  std::array<std::string, 2> keys{"LOG_LEVEL", "ENABLE_VALIDATION_LAYERS"};
  Core::Environment::initialize(keys);

  using mat4 = Matrix<4, 4>;
  std::array<mat4, 10> matrices{};
  auto buffer =
      Core::Buffer{matrices.size() * sizeof(mat4), Core::Buffer::Type::Storage};

  Core::Shader shader{Core::FS::shader("MatrixMultiply.comp.spv")};
  Core::Pipeline pipeline({
      "MatrixMultiply",
      Core::PipelineStage::Compute,
      &shader,
  });

  return 0;
}
