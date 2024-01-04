#include "Buffer.hpp"
#include "CommandBuffer.hpp"
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
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

#include <atomic>
#include <csignal>

template <Core::u64 N, Core::u64 M> struct Matrix {
  std::array<std::array<float, N>, M> data;
};

template <> struct fmt::formatter<Matrix<4, 4>> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const Matrix<4, 4> &matrix, FormatContext &ctx) {
    return format_to(ctx.out(),
                     "Matrix<4, 4>:\n"
                     "  [{:>8.4f}, {:>8.4f}, {:>8.4f}, {:>8.4f}]\n"
                     "  [{:>8.4f}, {:>8.4f}, {:>8.4f}, {:>8.4f}]\n"
                     "  [{:>8.4f}, {:>8.4f}, {:>8.4f}, {:>8.4f}]\n"
                     "  [{:>8.4f}, {:>8.4f}, {:>8.4f}, {:>8.4f}]",
                     matrix.data.at(0).at(0), matrix.data.at(0).at(1),
                     matrix.data.at(0).at(2), matrix.data.at(0).at(3),
                     matrix.data.at(1).at(0), matrix.data.at(1).at(1),
                     matrix.data.at(1).at(2), matrix.data.at(1).at(3),
                     matrix.data.at(2).at(0), matrix.data.at(2).at(1),
                     matrix.data.at(2).at(2), matrix.data.at(2).at(3),
                     matrix.data.at(3).at(0), matrix.data.at(3).at(1),
                     matrix.data.at(3).at(2), matrix.data.at(3).at(3));
  }
};

std::atomic<bool> interrupted(false);

void handler(int signum) {
  info("Interrupted with signal: {}", signum);
  // Set the flag to indicate the interrupt
  interrupted.store(true);
}

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

int main(int argc, char **argv) {
  signal(SIGINT, handler);

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
  randomize_span_of_matrices(matrices);

  for (const auto &matrix : matrices) {
    info("{}", matrix);
  }

  auto buffer =
      Core::Buffer{matrices.size() * sizeof(mat4), Core::Buffer::Type::Storage};

  Core::Shader shader{Core::FS::shader("MatrixMultiply.comp.spv")};
  Core::Pipeline pipeline({
      "MatrixMultiply",
      Core::PipelineStage::Compute,
      &shader,
  });
  Core::CommandBuffer command_buffer(3, Core::CommandBuffer::Type::Compute);

  Core::u32 frame = 0;
  while (!interrupted.load()) {
    randomize_span_of_matrices(matrices);
    buffer.write(matrices.data(), matrices.size() * sizeof(mat4));

    // Begin command buffer
    command_buffer.begin(frame);
    // Bind pipeline
    command_buffer.bind(pipeline);
    // Bind descriptor set
    // Bind descriptor set
    // Dispatch
    // End command buffer
    command_buffer.submit_and_end();
  }

  info("Exiting");

  return 0;
}
