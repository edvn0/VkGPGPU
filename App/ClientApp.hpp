#pragma once

#include "Allocator.hpp"
#include "App.hpp"
#include "Buffer.hpp"
#include "BufferSet.hpp"
#include "CommandBuffer.hpp"
#include "CommandDispatcher.hpp"
#include "DebugMarker.hpp"
#include "DescriptorMap.hpp"
#include "DescriptorResource.hpp"
#include "DynamicLibraryLoader.hpp"
#include "Environment.hpp"
#include "Filesystem.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "Logger.hpp"
#include "Material.hpp"
#include "Math.hpp"
#include "Pipeline.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Timer.hpp"
#include "Types.hpp"

#include <numbers>
#include <random>

#if !defined(GPGPU_PIPELINE)
#include <renderdoc_app.h>
#endif

using namespace Core;

class ClientApp : public App {
public:
  explicit ClientApp(const ApplicationProperties &props) : App(props){};
  ~ClientApp() override = default;

  void on_update(floating ts) override;
  void on_create() override;
  void on_destroy() override;

private:
  Scope<DescriptorMap> descriptor_map;
  Scope<CommandDispatcher> dispatcher;
  Scope<DynamicLibraryLoader> loader;

  Scope<CommandBuffer> command_buffer;
  Scope<Buffer> input_buffer;
  Scope<Buffer> other_input_buffer;
  Scope<Buffer> output_buffer;
  Scope<Buffer> simple_uniform;
  Scope<Material> material;

  Scope<Pipeline> pipeline;
  Scope<Shader> shader;
  Scope<Texture> texture;
  Scope<Texture> output_texture;

  std::array<Math::Mat4, 10> matrices{};
  std::array<Math::Mat4, 10> output_matrices{};

#if !defined(GPGPU_PIPELINE)
  RENDERDOC_API_1_6_0 *renderdoc{nullptr};
#endif

  auto compute(floating ts) -> void;
  auto graphics(floating ts) -> void;

  void perform();
};
